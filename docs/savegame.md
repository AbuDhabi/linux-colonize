# Original Colonization save games (`COLONY##.SAV`)

The DOS game writes `COLONY00.SAV`–`COLONY09.SAV` (slots 0–9). Autosaves use
slot 9 every turn and slot 8 every decade. This port models that binary layout
as `ColonizeCol1Save` in `src/core/col1_save.h`.

## Compatibility goal

Three layers — do not conflate them:

| Layer | Status | What it proves |
|-------|--------|----------------|
| **Codec** (`col1_save_read` ↔ `write`) | Strong | Byte-identical round-trip of original 3.0 fixtures |
| **Linux import** (`col1_bridge_apply`) | Strong for mapped fields | Originals load and play in the port |
| **Linux→DOS export** (`col1_bridge_capture`) | Strong for templates | Occupancy + density + blank census + colony levels; late `unknown_ds_*` / `other` stay zero/RMW |

`col1_save_read_*` / `col1_save_write_*` are intended to be **byte-identical**
round-trips of original 3.0 saves: every section is read into a packed struct
(or opaque buffer) and written back in the same order and size. That does **not**
mean a campaign save written after Linux play is DOS-safe.

Layout sizes are enforced by `col1_save_check_layout()` (also in
`unit_col1_save`).

**Header validation** mirrors DOS `FUN_75c2_0840` (slot probe / load):

| Check | DOS | Linux |
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

Total size formula:

```
390
+ 202*colonies + 28*units + 1264 + 18*tribes + 624 + 727
+ 4*map_w*map_h + 614 + 888
```

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
build/sav_json COLONY00.SAV COLONY00.json   # SAV -> JSON
build/sav_json COLONY00.json COLONY00.SAV   # JSON -> SAV
build/sav_json COLONY00.SAV                 # defaults to COLONY00.SAV.json
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
`tools/json_min.c` (generic JSON parser/writer).

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
| Colony `stock[]`, `hammers`, `building_in_production` | Simplified production |
| `nation[human].current_crosses` / `needed_crosses` | Crosses → dock immigrant |
| `nation[human].liberty_bells_*` | Bells counters (FF election stub) |
| Unit `moves` (via runtime MP) | Refreshed per nation phase |

Autosave (when `game_options.autosave` is set): slot **9** every turn, slot **8** when entering a decade Spring year.

Default save directory is `<exe>/COLONIZE` (via `savegame_default_dir()`), matching
the DOS install layout so Linux and DOS can share the same `COLONY##.SAV` pile.
The directory is created empty on startup if missing. Override with `--save-dir`.

Manual Save/Load (map menu, title **LOAD**, **S**/**L**) opens a wood slot popup:
- **Save** lists slots **0–7** (`COLONY00`–`COLONY07`); confirming overwrites the chosen slot.
- **Load** lists slots **0–9**; empty slots are not selectable; **8**/**9** are labeled as decade/turn autosaves when present.

Slot rows show `N. Empty` or `N. <leader>  <year>` from a prefix-only probe (`savegame_probe_col1_slot`).
If the chosen Load file is missing under the save dir, Load falls back to `original_saves/COLONY##.SAV` (repo samples).

Export is read-modify-write against the last loaded Col1 snapshot when present
(preserves tribes, unknowns, AI blobs). New games create a minimal template on
first Save. Before write, capture rebuilds `map.mask` `has_unit` / `has_city`
from live pools + `tribe[]`, and defaults unit `ai_plan` to `0x58` when
unset (`COL1_UNIT_UNKNOWN16_HI_DEFAULT`). Runtime `layer2` occupancy is kept
via `units_set_occupancy_map` on spawn/move/despawn.

### Remaining Linux→DOS gaps (known)

Full opaque-field inventory and RE phases: **[save_format_map.md](save_format_map.md)**.

**P6 bridge rebuild (done for playable templates):**

- Mask density: `col1_bridge_sync_map_density` — `pacific` / offshore `suppress`
  from terrain (`FUN_684c_08c0`); deplete/purchase from `layer2` / mask preserve
- Blank-template stuff census: `col1_stuff_census_fill_blank` (`FUN_4962_0018`);
  mid-campaign census still **DOS-parity preserved** (no freshen)
- Colony capture: specialty nibbles, `warehouse_level` / `capitol_level`,
  owner fog slot on `visible_to_euro` (smcol: whole `[4]` is fog **population**
  per Euro; +0xbe is fog **fortification** — see [save_format_map.md](save_format_map.md));
  Col1-only AI timers preserved by xy match
- Unit `vis_mask`: euro owner bit only on spawn/`units_set_nation`/capture;
  natives export 0 (fixes fog-visible Indians/AI). Ship `holds_occupied` =
  goods only — passengers use `transport_chain` (fixes fake food stacks).
  **2026-08-25**: the *import* side (`col1_bridge_apply`) wasn't honoring
  this on load — it copied all 6 `cargo_hold[]`/`cargo_item_*[]` slots
  whenever a slot's byte looked like a plausible amount, ignoring
  `holds_occupied`, so a ship that had unloaded (holds_occupied back to 0)
  but still carried stale nonzero bytes in the trailing array slots showed
  phantom goods. Fixed to only import the first `holds_occupied` slots
  (see `docs/report_screens.md`'s Naval report section for how this
  surfaced).
- Boarded euros: `origin=0xff` (`home_tribe_id=-1`), pioneer `cargo_hold[5]=100`
  tools; Discoverer English pioneer profession `28` (Hardy is French-only).
  Ship/wagon `profession=0` (`FUN_1427_06b4`); never `28` — ships must look like
  transports. Exception: while a unit runs a trade route (`orders=2`) DOS reuses
  that byte (unit `+0x17` = DS:`0x315b`) as the route cursor — low nibble =
  `trade_route[]` slot (`FUN_1427_0f64`/`0f74`), high nibble = stop index
  (`FUN_1427_0f8e`/`0fa0`), which `FUN_479b_0bd0` reads to bind the route before
  servicing a stop. The port unpacks it into `follow_unit_id` / `turns_worked`
  on import and repacks it on export (2026-09-04; before that the slot was never
  written to the file and every reload dropped the unit off its route). Idle on-map fleets export `goto==xy`; ships/aboard export `moves`
  as **moves_spent** (0 when full MP). Stale landfall with `orders=0` made DOS
  peel the caravel out of `transport_chain` (sidebar unloaded, land units left
  behind). 2026-08-29: `moves` is DOS spent thirds for every Euro unit —
  import `moves_left = max_mp − moves`, export `max_mp − moves_left`, with
  exhausted land units exporting 0 (DOS clears spent at the end of the
  nation's day; the TURN goldens show 0 there). Natives still round-trip the
  literal byte (the Brave engine keeps DOS spent in `moves_left`, max 3).
  Euro unit tiles stamp `map.path` / layer3 **owner** high nibble (`FUN_1427_02ca`
  / `FUN_137f_0228`) on spawn/move and capture — unowned ocean under a human
  fleet (`path=fx`) peels cargo on DOS select/move; COLONY00 / working patch F
  use owner `0` (`path=x1`). Capture nudges settler-ish euros off village tiles
  and boards same-tile orphans onto own ships (DOS "Illegal entry into village"
  / ocean sentry).
- Discovery: live play opens `@LANDHO` then sets `discovery_of_the_new_world` +
  `named_new_world` on confirm; capture/load still uses
  `col1_bridge_sync_new_world_discovery` as a safety net.
- **Human Europe ships (2026-08-29, P10.1).** The Europe screen's
  `harbor[]` / `expected[]` / `bound[]` lists are the only home of a human
  ship once it sails for Europe (`units_despawn_ship_with_cargo`), so capture
  used to drop them from the unit list. DOS keeps them as unit records on
  the nation's Europe sentinel diagonal (`FUN_48d3_007a` sail-to-Europe,
  `0346` sail-from-Europe, `03d0` per-turn tick): `228+n` in port,
  `232+n` sailing to the New World (`goto` = landfall), `244+n` sailing to
  Europe (`goto` = exit tile); unit `+0x16` (`turns_worked`) = voyage
  turns left; passengers chained `pax0→…→ship` with the same x/y/goto/turns,
  `orders=1`; Treasure `profession` = gold/100. Capture writes all three
  lanes that way (harbor passengers are already dock immigrants with their
  `(236,236)` mirror units, so only Expected/Bound carry cargo);
  `return_from_europe_x/y` ← `last_exit`. Apply classifies human ships by
  lane: `244+n` → Expected, `232+n` → Bound, with chained passengers kept
  aboard as `cargo_types`/`cargo_professions`; anything else at Europe
  coords stays the old harbor path (passengers to the docks, matching the
  Linux arrival model). Guarded by `unit_col1_save`'s recapture block
  (colony/unit counts survive apply→capture on all 19 fixtures; COLONY04
  Expected lane, COLONY06 harbor + synthetic Bound round-trip).
  **2026-09-03:** user loaded a Linux-written save with a ship at sea in DOS;
  it arrived.
- **Colony cap** raised 32 → 48 (`COLONIZE_COLONIES_MAX`): DOS's founding
  gate is `colony_count < 0x30`; five 33-colony lategame fixtures were
  silently truncated to 32 on apply.
- `post_map` connectivity still rebuilt on blank templates; `boot_timer` /
  `save_path_blob` stay zero / RMW-preserved

**Still RMW / zero-on-template (no DOS gameplay writer for rebuild):**

- Stuff late `unknown_ds_*` / `tribe_dwellings_91cc` (save I/O only in unpacked;
  blank dwellings → DOS “0 Villages” speech — still not rebuilt from `tribe[]`)
- `other[24]`, king / `price_group_state` overlay discipline (human prices only)
- Full mid-campaign AI nation blobs beyond fields AI already mutates in-place

**Fixture probe** (`tests-save-misc/unit flags error.sav`): occupancy orphans
cleared. Newgame template smoke checks density, blank census, `vis_mask`,
discovery sync, fleet `holds_occupied` / passenger `origin`+tools / ship
`profession=0` / native vis / idle fleet `goto==xy` / `moves` as spent /
ship-tile path owner nibble.
Archived `unowned units all visible.sav` was the DOS repro for the
vis/holds/discovery bugs; `loaded units still wrong.sav` for the unloaded
caravel / peeled transport_chain (pre-fix: ship `profession=28`,
`moves=4` as left, stale landfall `goto`, then `path=fx` under fleet).

### Verified fixtures

Codec byte-identical round-trip + import via `col1_bridge_apply`:
`original_saves/COLONY00.SAV`, `COLONY01.SAV`, all of
`valid-lategame-saves/COLONY{00–08,10}.SAV`, and AI `TURN1`–`TURN7` — every
Col1 `.SAV` fixture in the repo (12 lategame/TURN fixtures promoted to
`byte_identical=true` in `unit_col1_save`'s `k_fixtures` table).

**Phase 5 codec drift — resolved (2026-08-24, W1.5).** A 2026-08-22 pass
found lategame `valid-lategame-saves/COLONY{00–08,10}.SAV` and AI
`TURN1`/`TURN5`–`TURN7` not byte-identical on re-encode and documented it
here as open drift. Unnoticed at the time: the *same-day* commit `753662d`
"Fix FF + I work" (2026-08-22, 42 minutes after the drift note was written)
had already fixed it — `founding_fathers_stash_pools_into_col1`/
`_restore_col1_last_turn` gained a `saved_pad21` parameter that stashes and
restores nation `unknown21_pad` (the `FF_POOL_STASH_MARKER` byte) alongside
`liberty_bells_last_turn`; before that fix, a save write that stashed the FF
pool into `liberty_bells_last_turn` left `unknown21_pad` un-restored, so a
second write of the same in-memory save produced different bytes there.
Only the doc note calling it "drift" survived uncorrected. W1.5 re-verified
with a full-range byte diff (not just first-offset) across all 19 fixtures
(2 starters + 10 lategame + 7 TURN) plus `mapgen/SEED100.SAV` and
`tests-save-misc/unit flags error.sav`: zero differing bytes anywhere.
`unit_col1_save`'s `report_codec_roundtrip_diff` was also upgraded
same-pass to print every contiguous diff range (was: first byte only) —
kept as the tool for any future fixture that does regress.

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

- [save_format_map.md](save_format_map.md) — field atlas + RE roadmap (P0–P6 Done)
- [nawagers/Colonization-SAV-files `Format.md`](https://github.com/nawagers/Colonization-SAV-files) (upstream of hegemogy fork)
- [hegemogy/viceroy `savegame.h`](https://github.com/hegemogy/viceroy)
- [pavelbel/smcol_saves_utility](https://github.com/pavelbel/smcol_saves_utility) (`smcol_sav_struct.json`, `supplemental-info.md`) — community oracle; see atlas **Smcol audit** / **Nawagers audit**
