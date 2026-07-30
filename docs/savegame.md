# Original Colonization save games (`COLONY##.SAV`)

The DOS game writes `COLONY00.SAV`–`COLONY09.SAV` (slots 0–9). Autosaves use
slot 9 every turn and slot 8 every decade. This port models that binary layout
as `ColonizeCol1Save` in `src/core/col1_save.h`.

## Compatibility goal

`col1_save_read_*` / `col1_save_write_*` are intended to be **byte-identical**
round-trips of original 3.0 saves: every section is read into a packed struct
(or opaque buffer) and written back in the same order and size.

Layout sizes are enforced by `col1_save_check_layout()` (also in
`smoke_col1_save`).

Bitfield packing assumes **GCC/Clang on little-endian** (LSB-first), matching
DOS. Unknown regions are preserved as opaque byte arrays.

## Section map

| Section | Size | Notes |
|---------|------|-------|
| Prefix (`head` + `player[4]` + `other`) | 390 | Signature `COLONIZE`, counts, year/turn, options, REF |
| `colony[]` | 202 × colony_count | Buildings bitfield, stock, colonists, tiles |
| `unit[]` | 28 × unit_count | Map units / ships / braves |
| `nation[4]` | 316 × 4 = 1264 | Gold, tax, FF, Europe market |
| `tribe[]` | 18 × tribe_count | Indian villages |
| `indian[8]` | 78 × 8 = 624 | Per-tribe nation data |
| `stuff` | 727 | Viewport + unknown (incl. connectivity) |
| Map layers ×4 | `map_w × map_h` each | Standard 58×72 (56×70 visible + border) |
| `unknown_e` | 504 | 28 × 18 repeating records |
| `unknown_f` | 110 | Trailing unknown |
| `trade_route[12]` | 74 × 12 = 888 | Always 12 slots |

Total size formula:

```
390
+ 202*colonies + 28*units + 1264 + 18*tribes + 624 + 727
+ 4*map_w*map_h + 504 + 110 + 888
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

Save/Load (menu, **S**/**L**, REPORTS path) uses `COLONY00.SAV` in the save directory.
If missing, Load falls back to `original_saves/COLONY00.SAV` (repo samples).

Export is read-modify-write against the last loaded Col1 snapshot when present
(preserves tribes, unknowns, AI blobs). New games create a minimal template on
first Save.

Verified: `original_saves/COLONY00.SAV` and `COLONY01.SAV` byte-identical
round-trip through `col1_save_read/write`, and import via `col1_bridge_apply`.


## References

- [hegemogy/viceroy `savegame.h`](https://github.com/hegemogy/viceroy)
- [hegemogy/Colonization-SAV-files `Format.md`](https://github.com/hegemogy/Colonization-SAV-files)
- [pavelbel/smcol_saves_utility](https://github.com/pavelbel/smcol_saves_utility) (`smcol_sav_struct.json`)
