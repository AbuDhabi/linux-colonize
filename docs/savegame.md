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
| **Linux→DOS export** (`col1_bridge_capture`) | Partial | DOS can load/play only if occupancy and opaque defaults are sane |

`col1_save_read_*` / `col1_save_write_*` are intended to be **byte-identical**
round-trips of original 3.0 saves: every section is read into a packed struct
(or opaque buffer) and written back in the same order and size. That does **not**
mean a campaign save written after Linux play is DOS-safe.

Layout sizes are enforced by `col1_save_check_layout()` (also in
`smoke_col1_save`).

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

Default save directory is `<exe>/saves` (via `savegame_default_dir()`), overridable with `--save-dir`.

Manual Save/Load (map menu, title **LOAD**, **S**/**L**) opens a wood slot popup:
- **Save** lists slots **0–7** (`COLONY00`–`COLONY07`); confirming overwrites the chosen slot.
- **Load** lists slots **0–9**; empty slots are not selectable; **8**/**9** are labeled as decade/turn autosaves when present.

Slot rows show `N. Empty` or `N. <leader>  <year>` from a prefix-only probe (`savegame_probe_col1_slot`).
If the chosen Load file is missing under the save dir, Load falls back to `original_saves/COLONY##.SAV` (repo samples).

Export is read-modify-write against the last loaded Col1 snapshot when present
(preserves tribes, unknowns, AI blobs). New games create a minimal template on
first Save. Before write, capture rebuilds `map.mask` `has_unit` / `has_city`
from live pools + `tribe[]`, and defaults unit `unknown16[1]` to `0x58` when
unset (`COL1_UNIT_UNKNOWN16_HI_DEFAULT`). Runtime `layer2` occupancy is kept
via `units_set_occupancy_map` on spawn/move/despawn.

### Remaining Linux→DOS gaps (known)

Full opaque-field inventory and RE phases: **[save_format_map.md](save_format_map.md)**.

These survive capture today and may still fault or desync DOS play after the
occupancy fix:

- `post_map` (614) — zero on new-game templates; not rebuilt (sea/land
  connectivity + tallies; see [save_format_map.md](save_format_map.md))
- Most of `stuff` beyond viewport — counters only preserved on RMW; `unknown36`
  is FA/unit/tribe blobs, **not** connectivity
- Colony opaque fields (`unknown08`, `duration[]`, …) — zeroed on colony rebuild
- Mask `suppress` / `purchased` / `pacific` — not synthesized on pure templates
  (`COLONY00` has many bit5/`pacific` tiles; template exports often only occupancy)
- AI `nation[]` / `indian[]` blobs — only human gold/tax/crosses/prices updated
- `unused06` (nation high nibble) — preserved on apply→capture; spawn leaves 0

**Fixture probe** (`tests-save-misc/unit flags error.sav`): after occupancy
rebuild, `has_unit`/`has_city` orphans are gone (the `@UNITFLAG (47,14) (Arawak)`
case). The same file still has fully zero `post_map` and a mask
lacking DOS `pacific`/`suppress` density — next DOS load may pass UNITFLAG and
fail later, or play with missing opaque state. Do not invent those blobs without
decomp evidence.

### Verified fixtures

Codec byte-identical round-trip + import via `col1_bridge_apply`:
`original_saves/COLONY00.SAV`, `COLONY01.SAV`, and `test-saves-ai/TURN1.SAV`–`TURN7.SAV`.

Occupancy / export regression (`smoke_col1_save`):
`tests-save-misc/unit flags error.sav` (apply→capture must clear stray
`has_unit`), plus new-game template spawn→capture and `COLONY00` occupancy
sanity. Those checks do **not** claim full DOS campaign parity.


## References

- [save_format_map.md](save_format_map.md) — field atlas + RE roadmap (P0–P4)
- [hegemogy/viceroy `savegame.h`](https://github.com/hegemogy/viceroy)
- [hegemogy/Colonization-SAV-files `Format.md`](https://github.com/hegemogy/Colonization-SAV-files)
- [pavelbel/smcol_saves_utility](https://github.com/pavelbel/smcol_saves_utility) (`smcol_sav_struct.json`)
