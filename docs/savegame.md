# Original Colonization save games (`COLONY##.SAV`)

The DOS game writes `COLONY00.SAV`–`COLONY09.SAV` (slots 0–9). Autosaves use
slot 9 every turn and slot 8 every decade. This port models that binary layout
as `ColonizeCol1Save` in `src/core/col1_save.h`.

## Compatibility goal

Three layers — do not conflate them:

| Layer | Status | What it proves |
|-------|--------|----------------|
| **Codec** (`col1_save_read_file` / `_memory` ↔ `col1_save_write_file`) | Strong | Byte-identical round-trip of original 3.0 fixtures |
| **OpenCol import** (`col1_bridge_apply`) | Strong for mapped fields | Originals load and play in the port |
| **OpenCol→DOS export** (`col1_bridge_capture`) | Strong for templates | Occupancy + density + blank census + colony levels; late `unknown_ds_*` / `other` stay zero/RMW |

For an imported DOS map, capture retains the saved offshore/Pacific density
mask through the imported layer2 bits. Procedural density synthesis runs only
for a blank export template; applying it to the seed-100 idle campaign added
2,070 suppress bits that DOS did not write in the 1493 autosave.

`col1_save_read_*` / `col1_save_write_*` are intended to be **byte-identical**
round-trips of original 3.0 saves: every section is read into a packed struct
(or opaque buffer) and written back in the same order and size. That does **not**
mean a campaign save written after OpenCol play is DOS-safe.

Layout sizes are enforced by `col1_save_check_layout()` (also in
`unit_col1_save`).

**Header validation** mirrors DOS `FUN_75c2_0840` (slot probe / load):

| Check | DOS | OpenCol |
|-------|-----|-------|
| Signature | NUL-terminated `COLONIZE` then `0x1A` (`FUN_1b2c_*`) | `col1_save_validate_head` |
| Version | `uint16` == DS:`0x81a` (3.0 = **73**) | reject older → obsolete; newer → invalid |
| Map size | W×H product vs live map (`0`/`0` = any) | optional expect W/H; mid-game Load uses live map |
| File length | (implicit via section reads) | `col1_save_expected_size_counts` must match |

UI copy: `@LOADNOT` / `@LOADOLD` / `@LOADSIZE` in `GAME.TXT`. There is **no**
whole-file CRC on `.SAV` (CRC/LFSR `FUN_3f3f_0006` is unrelated). Header
checks alone do **not** catch map occupancy bugs — DOS raises `@UNITFLAG` /
`@COLONYFLAG` later, on first tile lookup (`FUN_1427_005c` / `FUN_15eb_0a76`).

Bitfield packing assumes **GCC/Clang on little-endian** (LSB-first), matching
DOS. Unknown regions are preserved as opaque byte arrays when RMW from an
original; pure new-game templates leave many of them zero.

## Section map

| Section | Size | Notes |
|---------|------|-------|
| Prefix (`head` + `player[4]` + `other`) | 390 | `COLONIZE\0` + `0x1A` + version **73** + map WxH + counts, year/turn, options, REF |
| `colony[]` | 202 × colony_count | Buildings bitfield, stock, colonists, tiles |
| `unit[]` | 28 × unit_count | Map units / ships / braves |
| `nation[4]` | 316 × 4 = 1264 | Gold, tax, FF, Europe market |
| `tribe[]` | 18 × tribe_count | Indian villages |
| `indian[8]` | 78 × 8 = 624 | Per-tribe nation data |
| `stuff` | 727 | 33 DS writes (`FUN_75c2_0288`); see [save_format_map.md](save_format_map.md) |
| Map layers ×4 | `map_w × map_h` each | Standard 58×72 (56×70 visible + border) |
| `post_map` | 614 | Sea/land connectivity 2×270 + continent tallies + 10 B tail (`ColonizeCol1PostMap`) |
| `trade_route[12]` | 74 × 12 = 888 | Always 12 slots |
| *port extension block* | 16 + chunks, optional | **Port-only**, past the last DOS section — see below |

Total size formula (the DOS-shaped part, `col1_save_expected_size`):

```
390
+ 202*colonies + 28*units + 1264 + 18*tribes + 624 + 727
+ 4*map_w*map_h + 614 + 888
```

`col1_save_total_size` adds the optional port extension block on top.

## Port extension block

State the port keeps but DOS has no field for lives in an optional block
appended **after** `trade_route[]`. DOS's loader (`FUN_75c2_0940`) reads the
file section by section and never checks the total length, so the tail is
invisible to it — and silently dropped when DOS re-saves that slot. The Load /
Save slot list therefore marks such files with a trailing `+`
(`ColonizeSaveSlotInfo.has_port_ext`, probed by reading the magic at the
DOS-shaped offset).

Layout (little-endian, `col1_save.h`):

| Field | Size | Notes |
|-------|------|-------|
| `magic` | 8 | `COLNXEXT` |
| `version` | 2 | `COLONIZE_COL1_EXT_VERSION` (1) |
| `reserved` | 2 | 0 |
| `payload_size` | 4 | bytes of chunk data that follow |
| chunks | `payload_size` | `uint32 tag`, `uint32 len`, `len` bytes, repeated |

Rules:

- `col1_save_read_*` accept a file/buffer **longer** than the DOS layout and keep
  the tail verbatim in `save->ext`; an unrecognised tail is logged and ignored,
  never an error (a DOS original still loads).
- `col1_save_ext_put` adds/replaces one chunk and carries every other chunk
  through untouched, so a newer port's chunks survive a round trip; removing the
  last chunk drops the block and the file is plain DOS-shaped again.
- The DOS-visible prefix is byte-for-byte what a plain save would hold.
- `sav_json` round-trips the block as an opaque `port_ext_hex` string, emitted
  only when present.

Chunks:

| Tag | Owner | Contents |
|-----|-------|----------|
| `VTIN` | `village_trade_intel.c` | Village sidebar `Buys:` / `Sells:` / `Skill:` knowledge per settlement tile × European nation (`uint16` version 2, `uint16` entry count, 40 B per entry; version 1's 36 B trade-only entries remain readable) |
| `UNXT` | `col1_save.c` | Unit records past `COLONIZE_COL1_DOS_UNIT_MAX` (300), written when `settings.json` `divergences.unlimited_units` let the game grow beyond DOS's `FUN_1427_06b4` limit: `uint16` original `head.active_unit` if it points into these records (else `0xffff`), `uint16` 0, then verbatim 28-byte unit records. Handled inside `col1_save_read_*` / `_write_*` only: in memory the save just has `head.unit_count > 300`. The DOS section keeps records 0..299 with `head.unit_count = 300`; tile-chain links into the overflow are cut to -1 and an overflow `active_unit` becomes `0xffff`. A DOS re-save drops the overflow units. A carrier in the DOS part whose passengers spilled still counts them in `holds_occupied` |
| `BQUE` | `colony_build.c` | Port-only per-colony build queues (`uint16` record count, then `x`, `y`, `count`, `count` project ids per colony, keyed by colony tile). QOL only — DOS keeps one project per colony |
| `PPLN` | `pioneer_plan.c` | Port-only Pioneer plan (`settings.json` `divergences.pioneer_plan`; written only while on): `uint8` version 1, `uint8` 0, `uint8` width, `uint8` height, width×height plan bytes (bit 0 Clear, 1 Plow, 2 Road), `uint16` count of automated Pioneers, then `x`, `y`, `nation` bytes per unit (keyed by tile, like `BQUE`) |
| `SAUT` | `explore_auto.c` | Port-only explorer Automate (`settings.json` `divergences.scout_autoexplore` / `ship_autoexplore`; written only while either is on and non-empty): `uint8` version 1, `uint8` 0, `uint16` count of automated Scouts and ships, `x`, `y`, `nation` (bit 7 set for a ship) per unit (keyed by tile), `uint16` count of entered settlements, `x`, `y`, nation bitmask per tile. DOS keeps only the unit's real order byte (its current Go To) |
| `RTST` | `runtime_ext.c` | Cross-turn state DOS keeps in DS but never saves (a DOS load starts it at zero; a continuous DOS session carries it), plus the port's LCG, so a port save/reload chain plays exactly like the continuous run. Records of `uint16 id`, `uint16 version`, `uint32 len`, payload; unknown ids / versions are skipped. Records (2026-10-19): 1 RNG (`move_rng` state, `ai_rng_seed` = the fixed FUN_281f_04ca tick word of a seeded run; unseeded runs read the clock instead), 2 Euro continent stance DS:0x9870 + rival strength DS:0x9e98, 3 unit-slot compaction residue (`ColonizeUnitPool.dos_tail`), 4 AI goal / work-queue / inventory / plan tables (`ai_goals.c`), 5 Euro rank cache DS:0x917c + profession census, 6 coarse fog plane DS:0x9faa, 7 Euro AI scratch (0a60 masks, explorers, village-visited bits, ship pressure, sticky anti-spin), 8 per-colony-slot `plot_was_worked_mask` (retired 2026-10-19: a load compacts the slots; no longer read), 9 pathfinder word DS:0x1dd2 + flood cache, 10 `plot_was_worked_mask` per colony in record order, 11 DS:0x8dc4 (`units_last_hold_amount`, the last FUN_15eb_317c slot amount), 13 xoshiro256** state (only with `settings.json` `divergences.modern_prng`; docs/settings.md). Written on every capture, so every port save now carries the block (and the `*` slot mark) |

Wiring: `col1_bridge_capture` serializes the live side tables into chunks,
`col1_bridge_apply` restores them (and clears the table when the save has no
chunk, i.e. a DOS original or a DOS re-save). Covered by `unit_col1_save`
(`test_port_ext`); the `RTST` carry is gated end to end by CTest
`golden_idle_reload` / `golden_idle_independence_reload` / `golden_idle_america_reload` (the idle campaigns
with a save/reload before every turn, `GOLDEN_IDLE_RELOAD=1`, must stay
byte-identical to DOS exactly where the continuous runs are). A save without
the chunk loads the way DOS loads: every table zero, LCG reseeded from the
turn number (`game_apply_col1_save`).

## API

```c
ColonizeCol1Save save;
col1_save_init(&save);
col1_save_read_file("COLONY00.SAV", &save, err, sizeof err);
/* mutate save.colony / save.unit / ... */
col1_save_write_file("COLONY00.SAV", &save, err, sizeof err);
col1_save_free(&save);
```

Slot helpers: `savegame_read_col1` / `savegame_write_col1` write
`<save_dir>/COLONY%02d.SAV`.

### CLI: SAV ↔ JSON

`tools/sav_json_main.c` (target `sav_json`) converts a `COLONY##.SAV` to a
JSON document field-for-field (this doc's section map + save_format_map.md's
atlas, named substructs for every bitfield/enum) and back:

```
build/debug/sav_json COLONY00.SAV COLONY00.json   # SAV -> JSON
build/debug/sav_json COLONY00.json COLONY00.SAV   # JSON -> SAV
build/debug/sav_json COLONY00.SAV                 # defaults to COLONY00.SAV.json
```

Opaque/pad byte blobs and the four raw map planes (tile/mask/path/seen) are
hex strings, not decoded further. Fixed-size name fields (colony/player/
tribe/trade-route names) round-trip as JSON strings up to their NUL
terminator — any DOS memory-reuse garbage *after* the NUL in the original
byte buffer is not preserved (invisible to DOS/the port either way; string
reads stop at NUL). Verified byte-identical round-trip on
`original_saves/COLONY00.SAV`/`COLONY01.SAV` and `mapgen/SEED100.SAV`; on
lategame fixtures the only diffs are that trailing-garbage-after-NUL case.
Implementation: `tools/col1_json.c` (struct↔JSON mapping) +
`src/core/json_min.c` (generic JSON parser/writer).

## Runtime game state mapping

`src/core/col1_bridge.c` maps Col1 ↔ live pools:

| Col1 | Runtime |
|------|---------|
| `map.tile` | `ColonizeWorldMap.terrain` (converted) |
| `map.mask` occupancy bits | Rebuilt by `col1_bridge_sync_map_occupancy` from units/colonies/tribes (not stale `layer2`) |
| `unit[]` | `ColonizeUnitPool` (incl. boarding chains) |
| `colony[]` | `ColonizeColonyPool` |
| `nation[human].gold/tax/prices` | `EuropeScreen` |
| `head.turn/year/autumn` | `turn_number` / `game_year` / `game_autumn` |

### Fields that tick on end-of-turn

Advanced by `turn_end()` in `src/core/turn.c` (and written back on Save):

| Field | Behavior |
|-------|----------|
| `head.turn` / `year` / `autumn` | `@TIMECHANGE` calendar |
| Colony `stock[]`, `hammers`, `building_in_production` | Colony production |
| `nation[human].current_crosses` / `needed_crosses` | Crosses → dock immigrant |
| `nation[human].liberty_bells_*` | FF bell pool / this-turn bells (save_format_map.md Nation) |
| Unit `moves` (via runtime MP) | Refreshed per nation phase |

Autosave (when `game_options.autosave` is set): slot **9** every turn, slot **8** when entering a decade Spring year.

Default save directory is `<exe>/COLONIZE` (via `savegame_default_dir()`), matching
the DOS install layout so OpenCol and DOS can share the same `COLONY##.SAV` pile.
The directory is created empty on startup if missing. Override with `--save-dir`.

Manual Save/Load (map menu, title **LOAD**, **S**/**L**) opens a wood slot popup:
- **Save** lists slots **0–7** (`COLONY00`–`COLONY07`); confirming overwrites the chosen slot.
- **Load** lists slots **0–9**; empty slots are not selectable. Slots 8/9 get **no autosave label** — DOS's own row builder (`FUN_7562_0052`) formats every occupied slot the same way and has no notion of an autosave slot.

Slot rows show `N. Empty` or `N. <leader>  <year>` from a prefix-only probe (`savegame_probe_col1_slot`); a trailing `+` marks a save carrying the port extension block (state DOS cannot load and drops on re-save). That probe runs the same header check DOS's lister does — `FUN_7562_0052` → `FUN_2a1f_0d04` → `FUN_75c2_0840`, i.e. `col1_save_validate_head`: signature, `0x1A` EOF marker and save version (map size skipped, as in the DOS probe path). A file that fails it lists as `(EMPTY)` rather than as a selectable slot (smell audit #82).
If the chosen Load file is missing under the save dir, Load falls back to `original_saves/COLONY##.SAV` (repo samples).

Export is read-modify-write against the last loaded Col1 snapshot when present
(preserves tribes, unknowns, AI blobs). New games create a minimal template on
first Save. Before write, capture rebuilds `map.mask` `has_unit` / `has_city`
from live pools + `tribe[]`, and defaults unit `ai_plan` to `0x58` when
unset (`COL1_UNIT_UNKNOWN16_HI_DEFAULT`). Runtime `layer2` occupancy is kept
via `units_set_occupancy_map` on spawn/move/despawn.

### Remaining OpenCol→DOS gaps (known)

Full opaque-field inventory: **[save_format_map.md](save_format_map.md)**.

**P6 bridge rebuild (done for playable templates):**

- Mask density: `col1_bridge_sync_map_density` — `pacific` / offshore `suppress`
  from terrain (`FUN_684c_08c0`); deplete/purchase from `layer2` / mask preserve
- Blank-template stuff census: `col1_stuff_census_fill_blank` (`FUN_4962_0018`);
  mid-campaign census still **DOS-parity preserved** (no freshen)
- Colony capture: specialty nibbles, `warehouse_level` / `capitol_level`,
  owner fog slot on `visible_to_euro` (smcol: whole `[4]` is fog **population**
  per Euro; +0xbe is fog **fortification** — see [save_format_map.md](save_format_map.md));
  Col1-only AI timers preserved by xy match
- Unit `vis_mask` (low nibble; DOS fog draw is `(0x10 << viewer) & vis_mask`):
  capture round-trips the live `col1_vis_mask & 0x0f` for **every** unit, then
  ORs in the owner's own bit for non-crown Euro units on the map — so a MAP
  euro record carries its owner bit (3004/3004 in the fixtures), while freshly
  landed REF units keep the target bit only (`FUN_43f7_0982` raw 74142/74210).
  A **native** record keeps whatever nibble it had (492 of 3074 on-map native
  records in the fixtures carry a nonzero nibble, values 1/2/4/8 and
  combinations: DOS really does track which Europeans have seen a given
  brave). **Do not "fix" this to zero natives.** Off-map records (`x`/`y` >=
  200) get no owner bit and export their live mask: 0 for most (a hull
  sailing for Europe is cleared, dock rows the port writes are 0), but a hull
  parked for repair keeps its map mask (crown Man-O-War at 236,236 saves
  vis 9 in the DOSBox independence trace).
  Ship `holds_occupied` =
  goods only — passengers use `transport_chain` (fixes fake food stacks).
  Import (`col1_bridge_apply`) reads only the first `holds_occupied` cargo
  slots: trailing `cargo_hold[]` bytes can be stale (see
  `docs/report_screens.md` "`col1_bridge_apply` transport-chain bugs").
- Boarded euros: `origin=0xff` (`home_tribe_index=-1`), pioneer `cargo_hold[5]=100`
  tools; Discoverer English pioneer profession `28` (Hardy is French-only).
  Ship/wagon `profession=0` (`FUN_1427_06b4`); never `28` — ships must look like
  transports. Exception: while a unit runs a trade route (`orders=2`) DOS reuses
  that byte (unit `+0x17` = DS:`0x315b`) as the route cursor — low nibble =
  `trade_route[]` slot (`FUN_1427_0f64`/`0f74`), high nibble = stop index
  (`FUN_1427_0f8e`/`0fa0`), which `FUN_479b_0bd0` reads to bind the route before
  servicing a stop. The port unpacks it into `follow_unit_id` / `col1_counter16`
  on import and repacks it on export. Idle on-map fleets export `goto==xy`; ships/aboard export `moves`
  as **moves_spent** (0 when full MP). Stale landfall with `orders=0` made DOS
  peel the caravel out of `transport_chain` (sidebar unloaded, land units left
  behind). `moves` is DOS spent thirds for every Euro unit —
  import `moves = max_mp − moves`, export `max_mp − moves`, with
  exhausted land units exporting their full allotment (DOS clears the
  `DS:0x3149` spent bytes once at the top of a calendar tick — [`year_loop.c`](../original_sources_annotated/turn/year_loop.c):140 —
  never at a nation's day end; DOS mid-turn saves like COLONY02.SAV show acted
  AI land units at full-or-overspent thirds, so never export 0 for a spent
  unit. Sentry/Fortified keep real MP overnight (port_bugs.md #715), so a
  zero is always a spend). Natives still round-trip the
  literal byte (the Brave engine keeps DOS spent in `moves`, max 3).
  Euro unit tiles stamp `map.path` / layer3 **owner** high nibble (`FUN_1427_02ca`
  / `FUN_137f_0228`) on spawn/move and capture — unowned ocean under a human
  fleet (`path=fx`) peels cargo on DOS select/move; COLONY00 / working patch F
  use owner `0` (`path=x1`). Capture nudges settler-ish euros off village tiles
  and boards same-tile orphans onto own ships (DOS "Illegal entry into village"
  / ocean sentry).
- Discovery: `@LANDHO` opens only from the human move executor's tail
  (`FUN_4720_049e`: a move whose destination 3x3 holds land while the mover's
  `named_new_world` is clear, `game_try_prompt_landho`) and sets
  `discovery_of_the_new_world` + `named_new_world` on confirm; capture, load,
  select and Europe arrival never set them (port_bugs.md #1178).
- **Human Europe ships.** The Europe screen's
  `harbor[]` / `expected[]` / `bound[]` lists are the only home of a human
  ship once it sails for Europe (`units_despawn_ship_with_cargo`); DOS keeps
  them as unit records on the nation's Europe sentinel diagonal, a **chain of
  lanes**, one hop per Atlantic tick (DOS derivation: save_format_map.md Unit
  `x`/`y` row, port_bugs.md #489): `236+n` = the Europe port, `228+n` = "lands
  at the very next tick" (the new-game fleet, `FUN_75c2_235c` raw
  121612-121646, starts there with counter 0), `240+n` = one hop short of
  Europe. Port mapping:
  `236+n` harbor, `228+n` Bound (`goto` = landfall), `244+n` Expected
  (`goto` = exit tile); unit `+0x16` (`col1_counter16`) = voyage
  turns left; passengers chained `pax0→…→ship` with the same x/y/goto/turns,
  `orders=1`; Treasure `profession` = gold/100. Capture writes all three
  lanes that way (harbor passengers are already dock immigrants with their
  `(236,236)` mirror units, so only Expected/Bound carry cargo);
  `return_from_europe_x/y` ← `last_exit`. Apply classifies human ships by
  lane: `244+n` → Expected, `228+n` → Bound (counter 0 kept verbatim there —
  it means "arrives next tick"), with chained passengers kept
  aboard as `cargo_types`/`cargo_professions`; anything else at Europe
  coords stays the old harbor path (passengers to the docks, matching the
  OpenCol arrival model). Guarded by `unit_col1_save`'s recapture block
  (colony/unit counts survive apply→capture on all 19 fixtures; COLONY04
  Expected lane, COLONY06 harbor + synthetic Bound round-trip). User-verified
  in DOS: a ship at sea in an OpenCol-written save arrives.
- **One tile chain per Europe lane (port_bugs.md #1158, 2026-10-23).** DOS finds
  a lane's units with FUN_1427_005c (the first record on the tile, then
  +0x315c to the chain's end) and walks +0x315e back, so a record on an
  unlinked segment is invisible: the dock painter FUN_38fd_15aa, the harbor
  ship counter FUN_38fd_0666 and the Atlantic tick FUN_48d3_03d0 all walk it.
  198 of the 200 multi-unit Europe lanes in `original_saves` are one chain (the
  2 others are in the hand-edited `COLONY00_no-transports.SAV`), ships and
  dockers together (dutch-reports.SAV 163 -> 125 hull -> 183 at 237). The
  painter draws the chain end first, so dock slot 0 is the newest record.
  Capture now writes the human's dock rows and harbor / Bound / Expected hulls
  before the chain pass, which links each lane as one chain by stack key:
  dock[0] newest, then the rest of the dock, then the harbor hulls (harbor[0]
  topmost), transit hulls in record order. A hull on its own segment is
  invisible to DOS ("No Ships In Port" with dockers present, a second transit
  hull never moved). Import reads the dock and harbor order from the same
  walk, and the dock unit's +0x314c board flag (1 board, 0 "Don't get on next
  ship"; FUN_1427_10be raw 8658-8664 boards a dock unit only on 1). DOSBox-X
  checked on `tests-automated-regression/europe_unequip_units/output.sav`
  (dock order and Caravel in port match). Port saves written before #1158
  load with the dock in DOS's (reversed) order.
- **Colony cap** raised 32 → 48 (`COLONIZE_COLONIES_MAX`): DOS's founding
  gate is `colony_count < 0x30`; five 33-colony lategame fixtures were
  silently truncated to 32 on apply.
- `post_map` connectivity still rebuilt on blank templates; `boot_timer` /
  `save_path_blob` stay zero / RMW-preserved

**Still RMW / zero-on-template (no DOS gameplay writer for rebuild):**

- Stuff late `unknown_ds_*` / `tribe_dwellings_91cc` (save I/O only in unpacked;
  blank dwellings → DOS “0 Villages” speech — still not rebuilt from `tribe[]`)
- `other[24]`, king / `market_demand_pool` overlay discipline (human prices only)
- Full mid-campaign AI nation blobs beyond fields AI already mutates in-place

**Fixture probe** (`tests-save-misc/unit flags error.sav`): occupancy orphans
cleared. Newgame template smoke checks density, blank census, `vis_mask`,
discovery sync, fleet `holds_occupied` / passenger `origin`+tools / ship
`profession=0` / native vis / idle fleet `goto==xy` / `moves` as spent /
ship-tile path owner nibble.

### Verified fixtures

Codec byte-identical round-trip + import via `col1_bridge_apply`:
`original_saves/COLONY00.SAV`, `COLONY01.SAV`, all of
`valid-lategame-saves/COLONY{00–08,10}.SAV`, and AI `TURN1`–`TURN7` — every
Col1 `.SAV` fixture in the repo (12 lategame/TURN fixtures promoted to
`byte_identical=true` in `unit_col1_save`'s `k_fixtures` table).

Full-range byte diff across all 19 fixtures (plus `mapgen/SEED100.SAV` and
`tests-save-misc/unit flags error.sav`): zero differing bytes. A regressing
fixture prints every contiguous diff range (`report_codec_roundtrip_diff`).
The old "Phase 5 drift" story: [archive/savegame_history_2026-10-24.md](archive/savegame_history_2026-10-24.md).

Mapped-field checks on starters + lategame (occupancy, `colony_counts` vs live
colonies, warehouse/capitol/depletion/timer bounds, `tiles[8..19]==0xff`,
`map_mode`/`zoom_level`, indian `contact_state` / `euro_relation_accum`,
nation tax/rebel sentiment, `prime_resource_seed`, connectivity planes).
Lategame COLONY00 also re-checks `FUN_67f4_0088` plane rebuild byte-exact.

Occupancy / export regression (`unit_col1_save`):
`tests-save-misc/unit flags error.sav` (apply→capture must clear stray
`has_unit`), plus new-game template spawn→capture and `COLONY00` occupancy
sanity. Those checks do **not** claim full DOS campaign parity.

**Lategame mapping notes:** `colony_counts[4]` matches live colony nations
exactly. `all_unit_counts` / `unit_type_counts` track euro units but can lag
(withdrawn AI rows stay stale) — that lag is **DOS behavior**; preserve it.
`warehouse_level` / `capitol_level` / `hammers_purchased` /
`rebel_sentiment` / `royal_money` populated as expected on developed colonies.


## References

- [save_format_map.md](save_format_map.md) — field atlas (RE phases P0–P6 all Done)
- [nawagers/Colonization-SAV-files `Format.md`](https://github.com/nawagers/Colonization-SAV-files) (upstream of hegemogy fork)
- [hegemogy/viceroy `savegame.h`](https://github.com/hegemogy/viceroy)
- [pavelbel/smcol_saves_utility](https://github.com/pavelbel/smcol_saves_utility) (`smcol_sav_struct.json`, `supplemental-info.md`) — community oracle; see atlas **Smcol audit** / **Nawagers audit**
