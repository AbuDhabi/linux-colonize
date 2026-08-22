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
- Boarded euros: `origin=0xff` (`home_tribe_id=-1`), pioneer `cargo_hold[5]=100`
  tools; Discoverer English pioneer profession `28` (Hardy is French-only).
  Ship/wagon `profession=0` (`FUN_1427_06b4`); never `28` — ships must look like
  transports. Idle on-map fleets export `goto==xy`; ships/aboard export `moves`
  as **moves_spent** (0 when full MP). Stale landfall with `orders=0` made DOS
  peel the caravel out of `transport_chain` (sidebar unloaded, land units left
  behind). Land/Brave `moves` still exported as moves_left (TURN golden compat).
  Euro unit tiles stamp `map.path` / layer3 **owner** high nibble (`FUN_1427_02ca`
  / `FUN_137f_0228`) on spawn/move and capture — unowned ocean under a human
  fleet (`path=fx`) peels cargo on DOS select/move; COLONY00 / working patch F
  use owner `0` (`path=x1`). Capture nudges settler-ish euros off village tiles
  and boards same-tile orphans onto own ships (DOS "Illegal entry into village"
  / ocean sentry).
- Discovery: live play opens `@LANDHO` then sets `discovery_of_the_new_world` +
  `named_new_world` on confirm; capture/load still uses
  `col1_bridge_sync_new_world_discovery` as a safety net.
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
`original_saves/COLONY00.SAV`, `COLONY01.SAV` only.

**Phase 5 codec drift (read→write, 2026-08-22):** lategame
`valid-lategame-saves/COLONY{00–08,10}.SAV` and AI `TURN1`/`TURN5`–`TURN7` are
**not** byte-identical on re-encode. `unit_col1_save` prints first diff offset
via `report_codec_roundtrip_diff` (smoke + mapping checks still run). Early
starters unchanged. Lategame diffs cluster in map/tail (offset ~10k+); TURN
fixtures differ from ~2k (units/colony band). No fixture flipped to
`byte_identical=true` until root cause is proven per field.

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
