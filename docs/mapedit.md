# Map editor (MAPEDIT.EXE)

STATUS: ported 2026-10-27 as the separate executable `opencol-mapedit`
(`src/mapedit_main.c`, `src/core/mapedit.c`). Source of truth:
`original_sources_decompiled/mapedit.c`; DS addresses below are MAPEDIT's own
(DS base = file 0x17470).

```
opencol-mapedit [--data-dir DIR] [--scale N] [-c | -m:FILE.MP | ?]
```

`-c` forces a new map, `-m:file` edits that file, `?` prints the usage
(`FUN_1000_28d8` / `27f6`). Maps live in the data directory, as in DOS (the
game's own `AMER2.MP` sits there).

MAPEDIT's own wording (the panel lines, "(More)", "UNTITLED.MP", the banner and usage,
"Exit value: %d") exists only in MAPEDIT.EXE's data segment. The editor reads it from
`COLONIZE/MAPEDIT.EXE` at run time (`mapedit_exe_str`, `MAPEDIT_DS_*`; a `%d` is substituted by
`mapedit_fmt_ints`, never by printf), so no MicroProse text is compiled in. Dialogs and menus
come from MAPEDIT.TXT / MAPMENU.TXT, terrain names from NAMES.TXT.

## Flow

- **Start** (`FUN_1000_247a`): with neither switch, the `@MAPTOEDIT` file
  picker. ESC, or no `*.MP` file, starts a new map; a load failure ends the
  program.
- **New map** (`FUN_1000_15fc`): `@NEWNAME`, max 20 chars, default
  `UNTITLED.MP`. The name is forced to `.MP` and upper case (`FUN_1c04_005c`).
  The map is always 58x72 ocean with zero layers 2 / 3 and starts dirty;
  `@XS` / `@YS` are never used.
- **Load** (`FUN_19f9_0170`): the version word must be 4 and W*H ≤ 12000.
  Then `FUN_1000_00b6` runs: hills and mountains drop their forest, and types
  16-23 fold to 8-15.
- **Save** (`FUN_19f9_02b0`, port `map_save_mp`): u16 W, u16 H, u16 version 4,
  then terrain, layer 2 and layer 3, W*H bytes each. Layers 2 / 3 are written
  as loaded or edited.
- **Exit** (menu, ESC, ^Q / ^X / Alt-Q / Alt-X): `@EXIT` when dirty
  (1 exit, 2 save and exit, 3 cancel). A failed save shows `@ERROR` and
  stays.

## Screen

| Part | Where | Source |
|---|---|---|
| Menu bar | y 0-7, MAPMENU.TXT through `map_menu_load_custom` | `FUN_1000_0196`: the code inserts the separators; "Memory check" and "About Map Editor" are never added, and "How To Use Maps" opens `@ABOUT` |
| Map | 240x192 at y 8, tile 16 >> zoom, (15 << z) x (12 << z) tiles | `FUN_1a47_0006`; rows / columns 0 and W−1 / H−1 never shown; a map narrower than the view is centred on wood |
| Minimap | wood frame (241, 8, 79x41), 56x39 tiles at (252, 9) | `FUN_1b56_03b4`; colour = centre pixel of each class's sprite |
| Panel | wood (241, 50, 79x150), FONTTINY from (242, 51) | `FUN_1000_094e`: Size, Curs, Terrain at cursor, Selected swatch + name + Shift help, Fill radius, Coast Protect |
| Cursor | ICONS.SS 0x13 + zoom (1-based), blinking every 20 BIOS ticks | `FUN_1000_1310` |
| Palette | full screen, swatches at (col*17+1, row*17+1), specials at y 0x30 | `FUN_1000_0e14` |

The compositor is the game's MAPEDIT one (`map_tile_layer_cmds`), with two
editor switches:
- no prime resources or rumours, because DS:0x4dc is 0 and never written
  (`ColonizeWorldMap.specials_off`);
- land transitions drawn only at zoom 0 (`FUN_1a47_0932`).

## Editing

- **Brush** (DS:0x56 type, 0x57 AND, 0x58 OR, 0x59 feature): starts as Ocean
  (0x19 / 0x40 / 0 / 0). Palette picks follow `FUN_1000_10cc`:
  - land types: AND 0xff;
  - Ocean / Sea Lane: AND 0x40;
  - major river 0x1f | 0xc0, minor 0x3f | 0x40, mountains 0x1f | 0xa0,
    hills 0x5f | 0x20, all as features.
- **One tile** (`FUN_1000_1be0`, `mapedit_paint_tile`, ported literally). Only
  tiles shown in the view are touched.
  - Coast Protect refuses every change to ocean / sea lane.
  - Hills and mountains never go on water; rivers do (the estuary marker).
  - Removing uses the feature's AND mask.
  - In terrain mode, Shift-Right / Backspace pick the tile up.
- **Brush square** (`FUN_1000_1d28`): (2r+1)², r = Fill radius 0-2.
- **Undo**: one 12000-byte snapshot of the terrain plane. It is taken before
  every Space / Enter paint, every feature Backspace, and every press on the
  map.
- **Keys** (`FUN_1000_1db6`):
  - arrows, Home / End / PgUp / PgDn and digits 1-9 (not 5) move the cursor;
    a move near a scrollable edge first recentres the view on the old
    position (`FUN_1000_145e`);
  - Space / Enter paint, Backspace picks up or removes;
  - the menu letters (S, A, L, Z, X, C, M, F, P, O, U) and F1-F4 (zoom levels).
- **Mouse** (`FUN_1000_20e0`):
  - map: the cursor follows a held button; a plain release, or a held right
    button, centres the view; Shift-Left paints, Shift-Right removes or picks
    up;
  - minimap release: centre there;
  - panel click: the palette.
- **Palette keys** (`FUN_1000_1226`): ported literally, including the odd
  edges (index 8 goes down a row on Up, 16 goes up on Down).
- **Find Continents** (`FUN_19c4_0002`, ported literally; differs from the
  game's `map_gen_assign_continents`):
  - layers 2 / 3 are not cleared first and nothing is ORed in;
  - a remapped size entry of 0 gives id 0;
  - more than 15 land masses / oceans gives `@CONTINENTS1` / `2`;
  - the result shows one pixel a tile at the origin (low nibble, water 0)
    until a key or click.

## Divergences

- The file picker lists `*.MP` sorted by name; DOS shows findfirst
  (directory) order.
- Panel text uses the @COLORS basic ink. DOS draws through the font's slot 0
  (`FUN_1d6a_0006(0)`), whose colour was not traced.
- The minimap view box and the Find Continents picture use reconstructed
  coordinates (DOS's blit arguments are lost in the decomp).
- The editor draws through TERRAIN.SS's palette, as the game's map does.
  MAPEDIT loads VICEROY.PAL.
