# Colonization Asset Notes

For a navigable index of decomp sources, `COLONIZE/` data files, and DOSBox memory
dumps, see [original_index.md](original_index.md).

Original game data (required to play) belongs in a `COLONIZE/` directory next to
the executable — the same layout as a DOS install. The port creates that folder
empty on startup if it is missing; copy the shipped MicroProse files into it.
Those files are **not** relicensed by this project; you must provide a legally
obtained copy. Override the search root with `--data-dir`. Saves default to the
same directory (see [savegame.md](savegame.md); override with `--save-dir`).

## Text Encoding

- Encoding: legacy DOS OEM / ASCII with printable 7-bit English.
- Line endings: CR/LF (`\r\n`). Parsers must accept both CR/LF and LF.
- Comment lines start with `;`.
- Section headers start with `@NAME` (e.g. `@BEGINMENU`, `@GAME`).
- Hotkeys are marked with `~` before the hotkey character (e.g. `~Save Game`).
- Directive lines inside sections also start with `@` (`@width=160`, `@default=2`, `@options`).

Key text catalogs:

| File | Role |
|------|------|
| `GAME.TXT` | Dialogs, main menu (`@BEGINMENU`), prompts |
| `MENU.TXT` | In-game map menu bar (`@GAME` `@VIEW` `@ORDERS` `@REPORTS` `@TRADE` `@CUP` cheat, `@PEDIA`; port adds DEBUG when `COLONIZE_DEBUG_MENU`) |
| `LABELS.TXT` | Short UI labels |
| `COLONY.TXT` | Colony name lists by nation |
| `ERRORS.DB` | Error / series name strings |
| `MODULES.DB` | Module tags |

## Binary / Packed Graphics

Many `.PIK` and `.SS` files begin with the ASCII header:

```text
MADSPACK 2.0\x1a
```

followed by:

| Offset | Type | Meaning |
|--------|------|---------|
| 0 | 12 bytes | `"MADSPACK 2.0"` |
| 12 | u16 | `0x001A` |
| 14 | u16 | section count (max 16) |
| 16 | 160 bytes | section headers (16 × 10) |
| 176 | ... | section payloads |

Each section header is `flags:u16`, `uncompressed_size:u32`, `compressed_size:u32`.
If `flags & 1`, the payload is FAB-compressed (`FAB` + shift byte + bitstream).

Colonization `.PIK` layout (after MADSPACK explode):

| Section | Content |
|---------|---------|
| 0 | Header: height, width, unk, unk (u16 each) |
| 1 | Indexed 8-bit pixels (`width * height`) |
| 2 | Optional VGA palette (768 bytes = 256×RGB, 6-bit DAC values) |

The Linux port decompresses MADSPACK/FAB and blits `.PIK` images. The main menu uses `OPENMENU.PIK` with its embedded palette, then overlays `@BEGINMENU` as a shared **popup window** (`OPENTILE.SS` fill, 3px wood bevel chrome from `src/core/popup.c`, `@width`/`@y`/`@smallfont` → `FONTTINY.FF`). Text colors come from `NAMES.TXT` `@COLORS` (`basic=68`, `{hilite}=149`, `select=138` on WOODPANL / in-game palettes; remapped by RGB onto `OPENMENU.PIK` so the title menu matches Colonizopedia greens — see `src/core/ui_colors.h`). Border colors (`border0/1/2`) are remapped the same way. Version line is `{COLONIZATION} Linux Port` + `COLONIZE_VERSION_STRING`. `CCBKGD.PIK` is the Continental Congress / Founding Fathers background.

### Popup window

Colonization uses the same wood dialog chrome for many confirmations and prompts. The Linux port exposes it as a reusable template in `src/core/popup.c` / `popup.h`:

| Layer (outside → in) | Size | Color (`NAMES.TXT` `@COLORS`) |
|----------------------|------|-------------------------------|
| Fill | full rect | Caller tile sprite 0 (`OPENTILE.SS` on the title menu; typically `WOODTILE.SS` in-game). Solid index `4` if no sheet. |
| Outer | 1px all sides | Black (`0`) |
| Mid | 1px all sides | `border0` = **134** (WOODTILE mid brown) |
| Bevel | 1px | `border1` = **128** light on **top + right**; `border2` = **138** dark on **bottom + left** |

Content draws inside the rect inset by `POPUP_FRAME_INSET` (3). Call `popup_draw(...)`; on palettes that differ from WOODPANL (e.g. `OPENMENU.PIK`), use `popup_colors_from_ui` + `popup_colors_remap`. First consumer: title `@BEGINMENU`. Not the same as `WOODFRAM.SS` (the milestone-woodcut picture frame, `woodcut.c`) or map-menu pulldowns. Which game dialogs use this chrome (and port status): [popups.md](popups.md).

`.SS` sprite sheets (e.g. `TERRAIN.SS`, `CURSOR.SS`) use four MADSPACK sections: header, per-sprite metadata, palette, and linemode-compressed pixel data. Sprites are blitted with transparency at index `0xFD`. `CURSOR.SS` #0 also has stray index `0x09` (light blue) at opposite corners; the SDL color cursor treats those as transparent.

`.FF` fonts (e.g. `FONTSMAL.FF`, `FONTTINY.FF`) are single-section MADSPACK files. After decompression:

| Offset | Size | Content |
|--------|------|---------|
| 0 | 1 | `max_height` |
| 1 | 1 | `max_width` |
| 2 | 128 | glyph widths for code points 1–127 |
| 130 | 256 | glyph offsets (u16 for code points 1–127, plus padding) |
| 386 | ... | 2-bit-per-pixel glyph data (4 pixels per byte) |

Palette indices used by glyphs: `0` = transparent, `1` = `0x0F`, `2` = `0x07`, `3` = `0x08`.

## World Map (`.MP`)

Colonization scenario maps (e.g. `AMER2.MP`) are raw binary files:

| Offset | Size | Content |
|--------|------|---------|
| 0 | 1 | map width |
| 1 | 1 | unknown (always 0) |
| 2 | 1 | map height |
| 3 | 3 | unknown header padding |
| 6 | W×H | terrain layer |
| 6+W×H | W×H | layer 2 (runtime flags; AMER2 is all zeros) |
| 6+2×W×H | W×H | layer 3 (fog / visibility in-game; rare art markers) |

**Layer 2** (MAPEDIT / VICEROY): bit 1 = settlement ownership (suppresses resources/rumours); bit 2 = depleted resource (silver → PHYS0 **89**). Shipped `.MP` files typically leave this zero; the live game fills it.

**Layer 3**: continent id (low nibble) / owner (high nibble) at runtime — never fog. AMER2 `(43,68)` carries `0x0e` because that one-tile island is continent 14; it is **not** a mountain marker (a former port hack drew PHYS0 32 there and painted lone peaks over every continent-14 tile in generated games — removed 2026-08-29).

Each terrain byte (FreeCol `ColonizationMapLoader` / MAPEDIT):

- bits 0–4: terrain index 0–26 (see **Map terrain index** below)
- bits 5–7: feature flags (also readable as FreeCol “overlay” 0–7):
  - `0x20` hill or mountain base
  - `0x40` river (any)
  - `0x80` with `0x20` → mountain; with `0x40` → major river; alone unused for land art

Forest indices 8–15 use bit 3; 16–23 use bit 4 (same eight forest types on cleared land 0–7).
The Linux port decodes the index as `byte & 0x1f`.

### Map terrain index (bits 0–4)

| Index | Type |
|------:|------|
| 0–7 | Cleared land (tundra…swamp; same order as TERRAIN sprites 0–7) |
| 8–15 | Forests on types 0–7 (bit 3 set); scrub = type 1 → indices 9, 17 |
| 16–23 | Same forests with bit 4 set |
| 24 | Arctic |
| 25 | Ocean |
| 26 | High seas |

Hills / mountains are **not** separate indices — they set bit `0x20` (and `0x80` for mountains) on a land/forest byte. Resource class then remaps to table slots **27** (mountain) / **28** (hill).

### TERRAIN.SS sprite index

| Sprite | Terrain type |
|--------|----------------|
| 0 | Tundra |
| 1 | Desert |
| 2 | Plains |
| 3 | Prairie (cotton) |
| 4 | Grassland (tobacco) |
| 5 | Savannah (sugar) |
| 6 | Marsh |
| 7 | Swamp |
| 8 | Scrub forest (indices 9 and 17 only) |
| 9 | Arctic |
| 10 | Ocean |
| 11 | High seas |

Indices 24/25/26 map to arctic/ocean/high-seas sprites. Other forest indices (8–23 except scrub) use the cleared-land sprite for their forest type (`index & 7`), with a `PHYS0` canopy overlay from the mixed-forest band.

### Graphic atlas browser (press `` ` `` in-game)

Scans `COLONIZE/` for every `.SS` and `.PIK`, loads one at a time, and shows file name + sprite index.

| Key | Action |
|-----|--------|
| Left / Right | Previous / next file |
| `[` / `]` | Jump −10 / +10 files |
| Up / Down | Scroll sprite grid, step sprite # (large sheets), or pan tall `.PIK` |
| Space / Enter | Page down |
| Esc / `` ` `` | Exit |

Small sprite sheets render as a labeled grid. Large or single-sprite sheets show one sprite centered with `FILE#index WxH`. `.PIK` images use their embedded palette when present.

**PHYS0 numbering:** atlas labels are **0-based** blit indices (0..153), matching `ss_blit_sprite` and the map compositor. MAPEDIT.EXE uses **1-based** IDs (1..154); isolated forest/hill/mountain are MAPEDIT `0x41`/`0x31`/`0x21` = atlas **64/48/32**. The PHYS0 HUD shows both (`#64 (=MAPEDIT 65)`).

`PHYS0.SS` contents (for map work):

| Sprites | Content |
|---------|---------|
| 0, 16 | Blank |
| 1–15 | Major rivers |
| 17–31 | Minor rivers |
| 32–47 | Mountains |
| 48–63 | Hills |
| 64–79 | Forests (`64 + mask`; isolated=64, all sides=79) |
| 80–88 | Roads |
| 89 | Depleted silver |
| 90 | Oasis |
| 91–95 | Wheat, cotton, tobacco, sugar, minerals/gems |
| 96–99 | Fish, beaver, deer, timber |
| 100 | Empty |
| 101–103 | Silver (mountain), ore (hill), rumours |
| 104–107 | Land-land transition colour-0 edge masks (N/E/S/W) |
| 108–139 | Coastline 8×8 fragments (`108+4*mask+q`; MAPEDIT `0x6d−1`) |
| 140–143 | Major river estuary corners (N/E/S/W) |
| 144–147 | Minor river estuary corners |
| 148 | Fog of war (unexplored tile fill; VICEROY `FUN_6ba1_0938` asm 6ba1:09a2 blits it as 1-based 0x95 — dark-blue noise of palette 60..62, one shade below the ocean tile's 58..60) |
| 149 | Plowed / other overlay |
| 150–153 | Coastal ocean corners (land NW/NE/SW/SE) |

### Map overlay compositing

**Fidelity status:** static AMER2 art matches MAPEDIT for coasts, estuaries, land–land transitions, forest/hill/mountain/river connectivity, special resources, and rumours. **Plowed fields** use runtime PHYS0 **149**; **roads** use PHYS0 **80** isolated or multi-blit **81–88** directional stubs (`FUN_6ba1_0938`). Land MP uses DOS `terr_cost` table at NAMES scale (`map_move_cost_*`). Remaining gaps are fog-of-war polish, coast animation, and per-tile texture variation (see [decomp_inventory.md](decomp_inventory.md)).

Authority for static map art is decompiled **`MAPEDIT.EXE` /
`original_sources_decompiled/mapedit.c`** (no RTLink), not VICEROY’s runtime buffers.
Compile-time toggles `MAP_COAST_OVERLAYS_ENABLED` / `MAP_ESTUARY_OVERLAYS_ENABLED`
(default **1** in `src/core/map.h`) exist only to disable coast/estuary for debugging.

The Linux port draws cleared terrain from `TERRAIN.SS` (bits 0–4), then composites `PHYS0.SS` in MAPEDIT order:

1. **Base** — land TERRAIN, or coastal **underlayer** (last cardinal land neighbour’s TERRAIN)
2. **Land transitions** (`FUN_1a47_06da`, land tiles only) — PHYS0 **104+q** colour-0 edge, then neighbour TERRAIN into holes; ocean neighbours resolve via land cardinals W→S→E→N
3. **Forest canopy** — PHYS0 **64+mask** (non-scrub)
4. **Overlays** — coast fragments/corners; hills/mountains/rivers; resources; rumours; estuaries  
   On coast tiles: coast PHYS0 → masked ocean into palette-0 holes → resource/estuary layers
5. **Plow** (runtime) — PHYS0 **149** when `map_tile_is_plowed` / Col1 plow bit
6. **Road** (runtime) — PHYS0 **80** isolated, else multi-blit **81+d** per connected 8-neighbor (`map_phys0_road_layer_*`)
7. **Fog fringe** — PHYS0 **104–107** on seen tiles toward unseen cardinals

**Terrain byte decode (layer 1):** `terrain_index = byte & 0x1f`. Indices 0–7 are cleared land; 8–23 are forests (type = `index & 7`); 24–26 are arctic/ocean/high seas.

**Forest TERRAIN/PHYS0 rules:**

| Forest type (`index & 7`) | Example index | TERRAIN sprite | PHYS0 overlay |
|---------------------------|---------------|----------------|---------------|
| 0 boreal | 8, 16 | 0 (tundra) | `64 + mask` |
| 1 scrub | 9, 17 | 8 | — (no canopy) |
| 2–7 other | 10–15, 18–23 | cleared type (`index & 7`) | `64 + mask` |

Non-scrub forests share one PHYS0 band (**64–79**). MAPEDIT connectivity mask bits: **N=8, S=4, W=2, E=1** (any non-scrub forest neighbour). TERRAIN still shows the cleared forest type. Sprite 99 is timber (bonus resource), not canopy.

Row `y=0` land tiles display as cleared tundra (sprite 0) with PHYS0 forest sprite 64.

**Hill / mountain / river overlays (shared adjacency, 0-based indices):**

| Feature | PHYS0 | Neighbour match |
|---------|-------|-----------------|
| Mountain | `32 + mask` | `(n & 0xa0) == (self & 0xa0)` |
| Hill | `48 + mask` | same |
| Major river | `0 + mask` (mask 0 → 15) | any `n & 0x40` |
| Minor river | `16 + mask` (mask 0 → 15) | any `n & 0x40` |
| Forest | `64 + mask` | any non-scrub forest |

MAPEDIT immediates are **1-based** (`0x21`/`0x31`/`0x41` → indices 32/48/64). Debug atlas labels match these 0-based indices.

Hills/mountains come only from the terrain byte; layer 3 never contributes art (the old AMER2 `(43,68)` "lone peak" reading of layer-3 `0x0e` was a continent id).

**Ocean estuaries.** Terrain index 25/26 with `terrain & 0xc0` marks river mouths; MAPEDIT blits IDs **141–148** → indices **140–147** toward land neighbours with bit `0x40`. See [decomp_inventory.md](decomp_inventory.md).

**Land terrain transitions.** After the base TERRAIN blit, MAPEDIT `FUN_1a47_06da` walks cardinal neighbours; when the neighbour’s display type differs (forests compared as `index & 7`), it blits PHYS0 **104+q** (colour-0 edge mask) then fills holes with the neighbour’s TERRAIN. Ocean/high-seas neighbours are resolved via their land cardinals (W→S→E→N) so coast corners pick up diagonal land fill. Drawn before forest canopy.

**Special resources / rumours.** Procedural from coordinates + seed (MAPEDIT `FUN_12ab_0458` / `0540`, default seed **100**). Type table at MAPEDIT DS:**0x4de** (file **0x1794e**; DS base **0x17470**). PHYS0 **89–102** = `89 + type`; rumours **103**. Not stored as art indices in `.MP` layer 2 (layer 2 only gates settlement / depleted).

| Type | PHYS0 | Typical terrain class |
|-----:|------:|------------------------|
| 0 | 89 | Depleted silver (layer2 bit 2 + type 12) |
| 1 | 90 | Oasis — desert, scrub forest |
| 2 | 91 | Wheat — plains |
| 3 | 92 | Cotton — prairie |
| 4 | 93 | Tobacco — grassland |
| 5 | 94 | Sugar — savannah |
| 6 | 95 | Minerals/gems — tundra, marsh, swamp, wetland/rain forests |
| 7 | 96 | Fish — ocean (25); high seas / arctic are −1 in the table |
| 8 | 97 | Beaver — mixed forest |
| 9 | 98 | Deer/game — boreal, broadleaf |
| 10 | 99 | Timber — conifer, tropical |
| 12 | 101 | Silver — mountains (class 27) |
| 13 | 102 | Ore — hills (class 28) |

Class index = terrain `& 0x1f`, except mountain → **27**, hill → **28** (`FUN_19b7_0006`). Table value `0` remaps to type 6; `−1` means no resource.

Roads blit via `map_phys0_road_layer_*` when `map_tile_has_road`: isolated
PHYS0 **80**, else one stub per connected 8-neighbor (**81**=N … **88**=NW;
`FUN_6ba1_0938` / MAPEDIT `FUN_1a47_0932`). Not a forest-style 16-mask.
Plowed tiles blit PHYS0 **149** via `map_phys0_plow_sprite_at`.

**Coastal ocean.** Enabled by default. MAPEDIT: land underlayer → fragments **108+4×mask+q** / corners **150–153** → masked ocean into colour-0 holes → estuary (+ fish when present). Details: [decomp_inventory.md](decomp_inventory.md).

| Piece | PHYS0 / TERRAIN |
|-------|-----------------|
| Underlayer + zero-fill | land TERRAIN, then ocean into dest==0 |
| Fragments + corners | 108–139 (8×8), 150–153 (16×16) |
| Estuary cardinals | 140–147 (16×16) |

Older VICEROY quadrant / RAM-buffer coast heuristics are **superseded** by this MAPEDIT path (`docs/viceroy_tables.md`).

Not drawn yet: per-tile texture variation from DOS RAM buffers; coast animation frames. Fog fringe (**104–107**), plow (**149**), and road **80–88** connectivity are drawn.

Tile compositing tables extracted from `VICEROY.EXE` live in `src/data/viceroy_tables.{h,c}`; see [viceroy_tables.md](viceroy_tables.md). World-map **feature art** (forest/hill/mountain/coast) uses MAPEDIT rules above, not those VICEROY tables.

### Map menu bar

On the main map, the top strip is the DOS menu bar from `MENU.TXT`: **GAME**, **VIEW**, **ORDERS**, **REPORTS**, **TRADE**, **COLONIZOPEDIA**, with fixed slots for **CHEAT** (`@CUP`) and (when built with `COLONIZE_DEBUG_MENU` and `debug.menu` is true) **DEBUG** between TRADE and COLONIZOPEDIA. The bar uses `WOODTILE.SS` fill (same as the right panel), `FONTTINY.FF`, `@COLORS` basic green with yellow (`~`) hotkeys, and a 1px black rule across the full width. Open pull-downs use the same wood fill (screen-aligned grain), a 1px black outline, green item text with yellow hotkeys, and sit **1px below** the bar rule so map pixels show in the gap. **Separators** are not in `MENU.TXT` (except PEDIA’s cosmetic `---`); DOS `FUN_74a4_0000` inserts empty-label rules into GAME / VIEW / ORDERS / REPORTS / CHEAT / PEDIA — the port mirrors those (plus one port-only rule between Founding Fathers and Miscellaneous in PEDIA, user-requested). ORDERS hide/gray follows `FUN_2b5a_0b34` (Move Pieces) / `0902` (View Pieces). Below the bar the screen splits into a **15×12-tile map viewport** (`x=0..239`, `MAP_PANEL_X` / `MAP_VIEW_*` in `src/core/map_panel.h`) and a **right info panel** (`x=240..319`, 80px) with a 1px black left edge. The world map is stored at 58×72 including a 1-tile rim; the viewport origin is inset by one tile (`FUN_6ba1_000c` / `map_panel_clamp_view_origin`) so the rim is never scrolled onto the main map (visible area 56×70). Click a title to open its pull-down; click an item to activate it (grayed items are stubs). Esc closes an open menu; Esc with no menu open returns to the title screen. Left-click on the map viewport: select an owned unit with moves, else select the tile (owned unit with no moves, empty land, etc.), or open an owned colony. Right-click always selects the tile (and clears unit selection). While a unit is selected, left-click only pans the viewport. Left-click on the panel minimap centers the view on that world tile. The selected tile shows a blinking white outline; over the map viewport the OS pointer uses `CURSOR.SS` #0.

**CHEAT:** Hidden until unlocked. On the main map, press **Alt-W**, **Alt-I**, **Alt-N** in succession (spells WIN); press **Alt-W** again to hide. Layout still reserves the CHEAT title slot while hidden so DEBUG does not shift. Enabled items: **Reveal Map** (Shift-F4) opens `DEBUG.TXT` `@SETVIEW` — English/French/Spanish/Dutch/Complete Map / No Special View. This is a reversible fog viewpoint (does not rewrite `seen[]`); Complete Map sets Col1 `show_entire_map`. **Kill Indians** (Shift-F6) picks a native nation (4–11) and removes its villages and units. Other CHEAT entries remain grayed.

**DEBUG** (CMake `COLONIZE_DEBUG_MENU`, shown when `debug.menu` is true): **Sprite Viewer** (same as `` ` ``), **Show Mouse Coords** (pointer pixel HUD; off by default, persisted as `debug.mouse_coords`), and **Building Rects** (colony-screen sprite bounds; off by default, persisted as `debug.building_rects`).

Working items today: Save/Load, Retire, Exit, **Pick Music**, European Status, Find Colony, Center View, Activate unit, Wait for next unit, Fortify (land/ship harbor) / Sentry / Disband, Clear Forest↔Plow / Build Road, Go to Place↔Port, Pillage, Dump Cargo Overboard, Begin Trade Route (sea/land-filtered route picker + starting-stop picker; DOS stop service with strict load/unload lists and Europe sell+auto-buy), TRADE Create (full DOS wizard: cap popup, destination pickers, sea/land choice, default name + entry) / Edit (full-screen EDIT TRADE ROUTE editor, `trade_screen.c`) / Delete (unit fixup + compaction), Build/Join Colony, Load/Unload Cargo (board/unload), Return to Europe, No Orders (end turn), full **COLONIZOPEDIA** menu (cargo / units / terrain / skills / buildings / fathers / misc; divider after terrain), **F1** terrain info at cursor, and **REPORTS** F2–F10. ORDERS plain-key hotkeys + Alt+menu titles.

### Main-map right panel

DOS layout: 15×12 main view (see `MENU.TXT` zoom levels) leaves an 80px strip. Implemented in `src/core/map_panel.c`:

| Element | Asset / source | Notes |
|---------|----------------|-------|
| Wood fill | `WOODTILE.SS` | Tiled over `x=240..319`, below the menu bar (and the menu bar itself) |
| Left rule | 1px black | `x=240`, full panel height |
| Minimap section | wood + 1px black separator below | Section is larger than the minimap; wood shows in the margins |
| Minimap | 56×39 window (1px/tile) | AMER2 visible interior (not full map / not rim); scrolls with the main view; origin inset like DOS; dark-orange border (palette 6) flush to section black rules; terrain/ocean/high-seas, colony (white) / tribe (palette 12) / unit dots; white view outline on edge tiles; click centers main view |
| Unit block | `ICONS.SS`, `LABELS.TXT` `@INFO`, `NAMES.TXT` `@NATIONALITY`/`@UNIT`/`@JOB` | DOS `FUN_49dd_0424`: chrome icon at x=242 with `Moves:` / `Locat:` at x=260 on an 18px row, then `<Nationality> <Type>`, the expert `@JOB` plural (highlight ink, only for types with a `DS:0x30e` profession slot), a Pioneers `(N Tools)` / Treasure `(Gold: N)` detail, and the orders line (goto appends the destination colony or terrain). No nameplate — DOS draws straight onto the wood |
| `With:` holds | `ICONS.SS` **#22–37** / greys **#38–53** | Commodity holds only, label and icons on the **same** row (icons start 1px past the label); sorted by Europe price × amount descending with Tools/Muskets forced last and Horses next-to-last; a hold below **100** draws the grey icon. Passengers are not icons here — they are stack rows |
| Tile stack | `ICONS.SS` + `NAMES.TXT` `@ORDERS` | Units on the tile **plus everyone aboard a transport on it** (COL1 keeps carried units in the same tile chain), re-ordered like `FUN_1427_04d6` (transports, then Treasure, then descending `@UNIT` size class). One 18px row each: chrome icon at x=242, text at x=260 — tools / profession / treasure gold / inline cargo icons / orders. Move Pieces skips the selected unit (it has its own block); View Pieces lists everything. A foreign-owned stack collapses to one `<Nationality> <Type>` row for the top unit, and only when its visibility bit is set. Listing stops at `y >= 0xb8` with the `@CTITLE` `(More)` marker |
| Date + gold + tax | Campaign calendar + `EuropeScreen` gold/tax | First row at y=0x33: `Spring 1492`; second `Gold:N$  Tax: N%` — DOS runs the amount straight onto its `@CTITLE` label with no separator, then two spaces before `Tax:`. Both rows are drawn in every mode, AI turns included (`FONTTINY.FF`, `@COLORS` basic) |
| Tile details | `NAMES.TXT` / WorldMap `improve` (Col1 mask fallback) / colonies / units | Under Locat: ownership (explored land only), `(Terrain)`, features (plow/road/river/resource/rumour), colony or native camp (`ICONS.SS` **#0–3** / **#10–13**). A fogged tile gets a single `(Unexplored)` line instead |
| Nation box | Turn indicator at `(315,197)` | 5×3 fill in the acting nation's color (`FUN_1984_00aa`), painted at the head of **every** nation's end-of-turn — the human's own production pass included |

Not used for this panel: `WOODPAN2.PIK` (score/fame chrome), `WOODFRAM.SS` (milestone-woodcut frame). Fog-of-war on the minimap is not drawn yet.

### Report / adviser screens

Open from **REPORTS** on the map menu bar, or press **F2–F10**. Esc (or Enter, or the shared bottom-right OK button) returns to the map. **F1 Terrain Information** opens the Colonizopedia entry for the terrain under the map cursor (not a report plate). Backgrounds:

| Key | Report | Background |
|-----|--------|------------|
| F1 | Terrain Information | Colonizopedia (cursor tile) |
| F2 | Religious Adviser | `REPORT2.PIK` |
| F3 | Continental Congress | page 1 `REPORT3.PIK`, page 2 `CCBKGD.PIK` |
| F4 | Labor Adviser | `REPORT4.PIK` |
| F5 | Economic Adviser | `REPORT5.PIK` |
| F6 | Colony Adviser | `REPORT6.PIK` |
| F7 | Naval Adviser | `REPORT7.PIK` |
| F8 | Foreign Affairs Advisor | `REPORT8.PIK` |
| F9 | Indian Adviser | `REPORT9.PIK` |
| F10 | Colonization Score | `WOODPANL.PIK` (full-screen wood) |

Content uses `ColonizeCol1Save` when a campaign is loaded (crosses / founding fathers / tribes / trade ledger / rival strength), with runtime colony / unit / Europe pools as fallback. **F10** uses the manual score schedule (citizen quality, congress, gold/1000, rebel sentiment, village-burn penalty, independence multipliers when declare/achieve are tracked). **F3 Continental Congress** is two pages: page 1 (bells progress bar toward the next Founding Father, rebel/tory split, expeditionary force, FF name list) advances to page 2 (full-bleed Founding Father group portrait, no chrome) on Esc/Enter/click/OK, and page 2 itself closes on any click.

All nine F2-F10 report plates are done to golden-screenshot fidelity (confirmed
2026-08-26; P2.12 user-passed 2026-09-03). See **[report_screens.md](report_screens.md)**
before touching any of them. Remaining gaps: a few hardcoded English strings
with no shipped `LABELS.TXT` line, F9 headband always #113, Hall of Fame has
no golden, Congress page-2 portraits all 25 draw from CC-xx.SS anchors (the
old 10/25 slot table is gone). See [reports.md](reports.md) "Current-state
note".

### Colonizopedia

Open from **COLONIZOPEDIA** on the map menu bar, or press **P** (Cargo Types list). Each category opens an encyclopedia **list** on `WOODPANL.PIK`: white **ENCYCLOPEDIA OF COLONIZATION** header, green clickable entry titles in up to three columns (`FONTTINY.FF`; Founding Fathers uses two wider columns), and **(Exit)** top-right. Click an entry for the article; Esc / (Exit) / P from the list returns to the map.

**Articles** are DOS-fidelity recreations of the segment-`6cb2` builders (`FUN_6cb2_05ce` cargo / `07e6` unit / `0eac` terrain / `1820` skill / `1ba8` building / `1f28` father / `203c` misc — `pedia_article_render` in `src/core/pedia.c`): WOODPANL background, centered hilite header, centered hilite `(Name: Category)` title (category subtitles from `@PEDIA`), per-category preview + stats, then the `PEDIA.TXT` section body (`@width` wrap, prose flows left-aligned in basic green, `^` lines centered on their own, `{...}` spans hilite). **Any key or mouse click dismisses an article** (DOS `FUN_281f_03c0` wait) — back to the list, or to the map when opened via F1. Category specifics: cargo pages show 1–3 producer rows (job figure + stacked goods icons + "Name (With Expert ...)", `FUN_6cb2_048c`); unit pages a chrome figure strip (Colonists = every profession, 17/row; Soldier-class types plain+veteran; Artillery plain+damaged) and a `Combat/Moves/Cargo Holds` stat line from `NAMES @UNIT`; terrain pages the double-framed 3×3 tile composite (forest/mountain/hill block pieces, river/road overlays, prime-resource center) plus per-job yield rows (`Plow|Road|Coast/River` and resource/Expert bonuses) and `Move Cost` / `Defense or Ambush Bonus` (`+25%×byte`); skill pages the expert figure, workplace upgrade chain from `BUILDING.SS`, and produced cargo; building pages the building art, worker + product row, and `Prerequisite:` line; father pages are text-only (DOS shows no portrait). DOS DS tables (job→chain `DS:0x2f4`, building→job `DS:0x2ca`, chain next/prereq, terrain→resource `DS:0x192`) were extracted from `dosbox-x-dumps/find_memory` and are hardcoded in `pedia.c`. Misc ("Game Concept") pages keep the port's short blurbs as bodies — DOS looks up `@MISC<n>` sections that don't exist in `PEDIA.TXT`, so its own misc pages are title-only.

The pull-down has a horizontal green rule between Terrain Types and Colonist Skills (DOS `FUN_74a4_0000` empty-label sep after Terrain; `MENU.TXT` `---` is ignored on load and the same sep is inserted in code).

| Menu item | PEDIA.TXT | Preview |
|-----------|-----------|---------|
| Cargo Types | `@CARGO0`–`15` | `ICONS.SS` #22–37 |
| Unit Types | `@UNIT0`–`22` (`@UNIT23` is a stub Colonists duplicate — dropped) | `ICONS.SS` (`NAMES.TXT` `@UNIT` icon) |
| Terrain Types | `@TERRAIN0`–`28` | 3×3 TERRAIN/PHYS0 composite |
| Colonist Skills | `@JOB0`–`27` (`@JOB18` Teacher/Student was cut pre-release — hidden from the list) | related cargo/unit icon |
| Colony Buildings | `@BUILDING0`–`41` | `BUILDING.SS` (Stable → sprite 46) |
| Founding Fathers | `@FATHER0`–`24` | none (DOS text-only) |
| Miscellaneous | `@MISCELLANEOUS` titles | text blurbs (no PEDIA bodies in data) |

**F1** / **REPORTS → Terrain Information** jumps straight to the terrain article for the cursor tile (Esc exits to the map).

### Europe (home port) bring-up

Press **E** from the map to open the European Status screen (`EUROPE.PIK`). Esc or E returns to the map (closes open menus first). Visual layout reference: [`original_screenshots/europe/`](../original_screenshots/europe/). Harbor / Expected / Bound ships and dock immigrants draw orders/allegiance chrome (`unit_chrome.c`) behind their `ICONS.SS` sprites.

Transit (`europe_voyage_turns_roll`, DOS `FUN_48d3_0002`): **1 turn**, or **2**
when `RNG(1,100)>89 && ships>2 && !Magellan` — both directions. The old
invented east-2 / west-4 table is gone (P9.2 / bugs.md 2026-08-31).

- **H** / Return to Europe on high seas → **Expected Soon** (not instant dock); docks when turns elapse (auto-opens Europe).
- New dock immigrants (crosses threshold or immigration pressure) show `@UNREST` and do **not** auto-open Europe — open with **E** when ready.
- New campaign: docks empty (recruit pool only); crosses start **0 / 9** until the first Europe EOT rewrites **needed** from the 584a population score (grows with colonists/units; English ×2/3). Idle **+2**/turn until the first dock immigrant.
- **S** / Set sail from harbor → **Bound For** `@COLONYNAME` region; arrives at last exit sea lane (also drag Loading → Bound).
- Drag Expected → Bound to reverse westbound (keeps remaining turns); drag Bound → Expected for the opposite reverse.

| Key | Action |
|-----|--------|
| **R** / 1 | Recruit menu — 3-slot pool; pay rising passage gold |
| **P** / 2 | Purchase — Artillery 500, Caravel 1000, Merchantman/Privateer 2000, Galleon 3000, Frigate 5000 |
| **T** / 3 | Royal University train — `@JOB` hire costs; expert to docks |
| **S** | Set sail selected/oldest harbor ship (Bound lane) |
| Click harbor / holds / market | Select ship; drag hold→market to sell; drag market→hold to buy |
| Drag Loading → Bound | Set sail (same as **S**) |
| Drag Expected → Bound | Reverse toward New World (keeps turns) |
| Drag Bound → Expected | Reverse toward Europe (keeps turns) |
| Click dock colonist | Dock orders (don’t board / board next / move to front) |
| **L** / `=` / `+` | Buy full / full / some of selected market good |
| **U** / `-` / `_` | Sell best hold / all / some |
| `]` / `[` | Cheat +1000 gold / −1% tax |

Market bid/ask from `NAMES.TXT` `@CARGO` (ask = bid + burden + 1). Sale proceeds `floor(bid × amount × (100 − tax) / 100)`. Volume-price T0 via `europe_apply_volume_price` (**Done thin**; EOT attrition + half `price_group_state`); boycotts **Partial** structural; pressure/bid chrome PARKED — [manual_gap.md](manual_gap.md).

### Colony screen bring-up

Press **C** on the map when the cursor is on a founded colony to open the colony screen. The layout matches DOS “THE COLONY DISPLAY”:

| Layer | Asset | Role |
|-------|-------|------|
| Wood chrome | `WOODPANL.PIK` (320×200) | Full-screen panel; supplies the colony-screen palette |
| Top bar | (rendered) | Colony name, campaign date, and treasury across the top strip |
| Settlement view | `PARCH.SS` + `BUILDING.SS` | Workers + badges via Note 1 strips at building bottom-center (selectable); hit-test uses **sprite bounds** (not fixed slot grid); outside units same on fence; thin construction banner (project + hammers; click opens Change) |
| Area view | `TERRAIN.SS` + `PHYS0.SS` + `ICONS.SS` | Centered 3×3 at **24px** tiles (1.5×); settlement **#0–3**; yields via resource-count strips (fisherman food uses fish **#57**) |
| Minimap panel | `WOODTILE.SS` (32×24 tile) | Wood-grain fill behind the 3×3 (equal L/R and T/B margins) |
| People view | `COLONY.PIK` left (x0–117) | SoL (flag **#123**) / Tory (crown **#124**, right-aligned); colonists + fence units on one row; food→crosses→bells resource strips (fish **#57** first, then grain food) |
| Transport view | mid (x121–202) | Ship **class** name; holds: color / grey partial / empty cover **#122** (all six covered with no ship); docked ships + passengers via `unit_chrome_blit_unit` |
| Multifunction | right (x207–306) + buttons (x307–318) | Production / Units / Construction (house **#67** / rifle **#68** / hammer **#69**); Production packs all cargo+shortfalls+hammers; **Units** rosters land units at the colony — colonist-class + Artillery, not an armed-only subset, but never ships/wagons (those stay on the Transport strip); 2nd click opens the same docked-unit orders popup as the Transport strip ([`colony_screen_multi_units_layout`](../src/core/colony_screen.c); DOS `FUN_2f2b_1e46`/`59a0` share `FUN_2f2b_5746`); Construction shows accumulated hammers |
| Warehouse | cargo strip `y=180/192` | Unchanged slots (`ICONS.SS` **#22–37**); Exit `x≥305` |

**Resource-count strip** (`colony_screen_draw_resource_count` / `_pair` / `draw_icon_strip`): one icon per unit, evenly spaced in a rect; when start-to-start spacing is ≤ 1px (nearly total overlap), overlay the amount on the **left** with a black outline and a caller-chosen foreground (white / green / red / …). An `always_show_number` flag exists but is unused by current call sites. Used for area yields, people meters, Production tab, Construction accumulated hammers (two rows of progress toward the project, not total cost), and settlement building/fence colonists. Fisherman yields still count as food cargo; colony UI only swaps the icon to `ICONS.SS` **#57** (fish before grain when both appear in one strip). Colonist/unit strips pass a `selected_index` so individual icons are selectable; resource strips pass `-1`.

`CLOS-BKG.PIK` is the independence closing-sequence backdrop (CLOSING.EXE / `closing.c`) — not used on the colony screen.

Single-pixel black separators split the sections: top bar vs middle band, buildings vs minimap, and middle band vs bottom panel.

A new colony gets the classic free starters: Town Hall, Carpenter's Shop, Blacksmith's House, Weaver's / Tobacconist's / Distiller's / Fur Trader's houses. Warehouse, Stockade, and Docks are **not** free. Warehouse cargo starts empty (DOS `FUN_364b_1ba8` clears stock); founding unit gear still dumps tools/muskets/horses. Founding defaults `building_in_production` to **Stockade**. Until Stockade is built, the fortification strip shows the post-and-rail fence (`BUILDING.SS` **#16**) bottom-right. Coastal colonies without Docks show empty coast (`BUILDING.SS` **#45**) above the fence. Empty building slots use tree clumps (`BUILDING.SS` 42–47). Founding with **B** disbands the map unit into a Town Hall colonist (skill/`profession` preserved) and dumps carried tools/muskets/horses into the warehouse.

**Drag or select-then-click** assignment: drag a colonist (or fence unit) onto a building, area/minimap tile, or the fence; short click still selects, and a second click assigns. Outside units are admitted into the colony on assign; carried tools/muskets/horses go to the warehouse. Drop/click the fence with a colony colonist selected to open a **Leave as** popup (Colonist / Pioneer / Soldier / Scout / Dragoon / Missionary if Church or Cathedral, gated by stock). Working colonists use skill sprites (Hardy Pioneer **#58**, Veteran Soldier **#59**); outside/map icons follow equipment (pioneer/soldier/scout/dragoon; expert sprites only when `profession` matches that role — e.g. hardy pioneer with muskets uses non-veteran soldier **#74**). Skill sticks across admit/eject and gear changes. The people band shows colony colonists and fence/on-tile units on the same row (outside group to the right, with a gap). Production preview (`colony_preview.c`) drives area/settlement badges, people meters, and the Production tab without mutating stock. Town Hall / Church / Cathedral settlement badges include free bells/crosses even with no worker assigned. The Production multipurpose pane lists every produced cargo type (grey shortfall rows) plus hammers, packing slots into a dynamic grid as the set of types changes; crosses/bells stay on the people meters. With no docked/selected transport, all six hold slots show empty-hold covers. SoL uses Col1 `rebel_dividend`/`rebel_divisor` when the colony is bridged from a save; otherwise 0% SoL / 100% Tory.

| Key / click | Action |
|-------------|--------|
| Click / drag colonist → building / area / fence | Select; assign workplace, open field-jobs popup, or Leave-as |
| **N** / click Production pane | Toggle production numbers on dense strips |
| **1** / **2** / **3** / **M** | Multifunction Production / Units / Construction / cycle |
| Construction **BUY** / **CHANGE** | Buy remaining project (multifunction BUY); Change popup lists projects only (no Buy row; **C** also opens Change) |
| **B** / Buy | Finish current project: gold = remaining hammers; spend `tools_cost` from warehouse |
| Drag / click ship cargo / hold | Select transport; drag warehouse↔hold to load/unload |
| Fence **Leave as** | Eject/equip popup (Colonist / Pioneer / Soldier / Scout / Dragoon / **Missionary** if Church/Cathedral); last colonist confirms **abandon** (cargo lost); Stockade/Fort/Fortress cannot drop below 3 colonists |
| **L** / **U** | Load highest-value cargo; unload first hold |
| **=** / **+** | Full / partial load of selected warehouse cargo |
| Esc | Close message / jobs / eject / construction, then return to map |
| Enter | Confirm popup; with a colonist selected open field jobs; else leave |
| Space | Free production turn; status shows food/lumber/ore/hammer deltas |

### Units (map bring-up)

Unit stats come from `NAMES.TXT` `@UNIT` (icon index, movement, land vs sea via hull field). Map markers use `ICONS.SS` sprites; the `@UNIT` icon field is **1-based** (DOS style), converted to 0-based blit indices on load (e.g. Pioneers `102` → sprite **101**, Caravel `6` → **5**). Cargo strip icons `#22–37` are already 0-based (greys `#38–53`). European colonies blit settlement art **#0–3** (none / stockade / fort / fortress; 21×16, centered on the tile) with a name label under the tile; Indian villages blit **#10–13** by `@TRIBES` tech (tipis / adobe / pyramid / city). Draw order is colonies → villages → units so stacked units stay visible.

`@UNIT` row names are plural catalog labels (`Colonists`, `Pioneers`, `Soldiers`, `Dragoons`, `Scouts`, …), not display strings — `units_display_name()` derives the singular UI name from base type + equipment + profession (armed→Soldier/Veteran Soldier, mounted→Scout/Seasoned Scout, tools→Pioneer/Hardy Pioneer, else `Pioneers`→Pioneer / `Soldiers`→Soldier / **`Colonists`→Free Colonist**). The bare-`Colonists` branch is load-bearing: every `strstr(units_display_name(...), "Free Colonist")` gate in the port (`ai_contact` teach-skill, `ai_euro` LABOR joins / founding-site picks, …) depends on it to recognize an ordinary colonist against real game data — there is no separate `Free Colonist` row in `@UNIT`.

**Orders / allegiance chrome** (`unit_chrome.c`, DOS `FUN_112b_01ba`): each unit graphic is a black silhouette shifted 2px left, then a nation-colored orders rectangle (`@ORDERS` letter), then the color sprite. Placement by Col1 type: foot units bottom-right; mounted / Caravel / Merchantman top-left; Galleon–Man-O-War top-right; Treasure / Artillery / Wagon top-center. A second under-rect marks stacks. England fill uses palette **112** (saturated red); NAMES `@COUNTRY` lists 12, which is pink `(255,85,85)` in VGA art palettes. Wired on the main map, sidebar, Europe docks/ships, colony Units + transport panes, and Colonizopedia unit previews.

Terrain move costs (plains 1, forest/hills 2, mountains 3; road/river halves) follow DOS `FUN_465b`: a unit may always enter when it still has its **full** movement allotment (remaining MP then exhausts). With **partial** MP into an over-budget tile, the game rolls `range(1, cost)` and succeeds if `roll ≤ remaining`; the full cost is charged either way (failed rolls leave the unit in place with 0 MP). Go-To pathfinding only commits guaranteed steps (no gambling).

| Key | Action |
|-----|--------|
| Arrows / numpad | Tile mode: move blinking tile cursor. Unit mode: move selected unit (numpad diagonals supported; MP cost from terrain / road / river) |
| Enter | Select movable unit under cursor, or move selected unit toward cursor tile |
| Click | No selection: stack popup if multiple; else select unit / tile. With unit selected: **drag** to set Go-To (CURSOR.SS #1 after ≥1 logical pixel); short click pans (or enters own colony). Right-click clears selection |
| C | Enter colony screen when cursor is on a colony tile |
| D | Deploy oldest Europe-dock immigrant as a Colonist on cursor tile (land only) |
| O | Board: selected land unit onto ship under cursor, or selected ship loading land unit under cursor (adjacent; boarded units go on sentry) |
| U | Unload oldest passenger from selected ship onto adjacent enterable land under cursor |
| H | Ship selected: sail to Europe (must be on a high-seas tile, terrain index 26); passengers stay aboard in harbor. No ship selected: VIEW **Show ~Hidden Terrain** — peel units/settlements, then non-exempt land PHYS, then hills/forest (see manual_gap.md) |
| P | Pioneer selected with moves: plow/clear (20 tools). Otherwise open Colonizopedia |
| R | Pioneer selected with moves: build road (20 tools). Otherwise unused on map (Europe: recruit) |
| B | Found a colony on the cursor tile: land unit is disbanded into a colony colonist; tools/muskets/horses go to the warehouse stub (ships cannot found) |
| Space | End turn (`src/core/turn.c`): advance calendar (`@TIMECHANGE`), run colony production + nation ticks, Euro/Indian AI + King/REF, refresh human movement |
| ORDERS → Wait | Select next human unit with remaining moves (“Continue turn.”); if none and End of Turn option is set, show “End of Turn” |
| End of Turn prompt | `LABELS.TXT` `@MISC` line 3, drawn once the unit cycle runs dry (DOS `DS:0x53c6`). Sits in the running sidebar text flow, clamped so it never starts below `0xc6` minus one line, alternating white (15) / black (0) on the **same** blink phase as the map tile cursor (DOS `DS:0x929c`). While it is up the next left click — on the map viewport or the sidebar — confirms the turn (`FUN_2b5a_3752` tests `0x53c6` before dispatching the click anywhere else); minimap clicks still re-centre |

Ship→**native village**: never landfall — unmet `@DONTKNOWSHIPS`, met/angry `@MADATSHIPS` (DOS `4528` ship abort). Ship→**non-colony bare land** with cargo: **landfall CHOICE** (`@LANDFALL` Stay With Ships / Make Landfall); prefers a passenger with moves, but aboard **sentry** cargo is still eligible (DOS spent==0). Make Landfall unloads **one** passenger, charges dest terrain MP from their allotment (or remaining moves), and **−1** ship MP; ship stays at sea. Ship→**own colony** land: dock and **disembark all** (clear sentry). Land→ocean with an own ship that has room: **board**. Sentry land units on a ship's tile **auto-board** when that ship leaves (colony or ocean stack). Multi-unit tile click opens a wood **stack popup** — first click wakes sentry cargo, second selects; awake cargo can walk onto land to disembark. **O**/**U** remain. Enter rules: [move_enter.md](move_enter.md).

Selected units **blink** (sprite on/off), except while executing **Go-To** (always drawn so pathing stays visible). The tile cursor is shown only when no unit is selected. Units with `moves_left == 0` cannot be selected (tile under them is selected instead). Awake passengers (sentry cleared) with moves can be selected from the stack popup. When the active unit spends its last move, the next human unit with moves is selected; if none remain, tile-select mode resumes.

**Go-To:** drag from a blinking unit to a destination tile (CURSOR.SS #1 appears after ≥1 logical pixel of drag; pathfinding uses DOS-style destination cost flood nearby, BFS farther), or ORDERS **Go to Place** (click destination) / **Go to Port** (next owned colony). The unit walks at **10 steps/sec** until out of moves and resumes after end of turn; **Fast Piece Slide** shortens the interval to 80ms (12.5 steps/sec). During interactive AI turns, **Show Indian Moves** / **Show Foreign Moves** present nearby AI tile steps using the same pacing. Sub-pixel slide animation remains final polish. Order byte is `@ORDERS` index 3.

Calendar: one turn per year until **1600**, then Spring and Autumn each year. Colony top bar / reports use `game_year` + `game_autumn`. When the Col1 **Autosave** option is set, end-turn writes slot **9** (and slot **8** on decade Spring years).

A **5×3** nation-color box appears in the bottom-right `(315,197)` only while end-of-turn
nation phases run (`FUN_1984_00aa`): England **112** (saturated red; NAMES lists 12), France 9, Spain 14, Netherlands 13
(`NAMES.TXT` `@COUNTRY`). Native phases use `@TRIBES` colors. It is hidden during the
human turn.

A starter **Caravel** (Dutch: **Merchantman**) with a Pioneer and Soldier aboard
spawns on the western rim of the eastern high seas when finishing the new-game wizard.
Skills follow classic COL1 rules: Hardy Pioneer is **French-only** on every difficulty;
Discoverer/Explorer grant Veteran Soldier to all nations; on harder difficulties only
the Spanish keep Veteran Soldier. See [difficulty.md](difficulty.md). Rival Europeans
spawn the same way. Europe keeps dock
immigrants until deployed with **D**. Press **B** with a land unit on a land tile to found
a colony (unit becomes a Town Hall colonist; name comes from `COLONY.TXT @ENGLISH`).
Ships move on water and may enter **own-nation colony** land tiles (dock). Land units
only on land. Boarded units are hidden from the map until unloaded (or woken via the
stack popup / landfall / colony disembark).

### New-game wizard

Title `@BEGINMENU` no longer jumps straight to the map. Flow (see `src/core/new_game.c`):

1. **Start in NEW WORLD** → difficulty → … → sail → **procedural 58×72 map** (`map_generate` with randomized `MapGenParams`; Euro starts via `map_gen_euro_landfall` / `FUN_684c` HS rim)
2. **Start in AMERICA** → `@AMERICA` (Original Americas = AMER2, or Map Editor `*.MP` list) → same wizard → load that `.MP` + `@SCENARIO` starts
3. **CUSTOMIZE New World** → `CUSTOMIZ.PIK` 4×3 grid (`FUN_733a_0270`; Land Mass / Form / Temperature / Climate) → same wizard → `map_generate` with user params (`forest_extra` stays 1)
4. Difficulty (`DIFFICUL.PIK` image regions + 1px border, click to select / Enter or finished to confirm) → nation (`NATIONS.PIK`, same) → leader name on `WOODPANL.PIK` (unbold `FONTINTR`, green input box; the field itself is the shared text widget — see [Text input fields](#text-input-fields)) → `@NATION{n}A` / `B` on wood (same unbold `FONTINTR`, green+shadow; body lines flow-wrap to `@width` like DOS `FUN_6f74_1198`, vertically centered) → king audience → `LEVN0001`–`0010.PIK` + `@BUILD1`–`10` yellow captions → map


### Text input fields

`src/core/text_edit.c` is the single implementation behind every typed field —
the leader name in the new-game wizard and the `@COLONY` / `@RENAMECOLONY` /
`@LANDHO` name dialogs. Both callers own the `char[]` and a `TextEditState`
(caret + selection anchor), hand them to `text_edit_handle_input` each frame and
to `text_edit_render` at draw time.

Colours come from `NAMES.TXT` `@COLORS` (`src/core/ui_colors.h`), under the
`WOODPANL` / in-game palette:

| role | @COLORS | index | RGB |
| --- | --- | --- | --- |
| text | `basic` | 68 | 85,150,52 (green) |
| drop shadow | `enhance` | 128 | 121,73,52 (brown-orange) |
| selection fill | `select` | 138 | 60,32,24 (dark brown) |

Note the field's shadow is the warm `enhance` brown, not the black
`popup_draw_text_shadowed` puts under the surrounding captions. Both fields are
boxed by the same 1px green rectangle (`text_edit_draw_frame`, `basic` ink), laid
out by `text_edit_frame_rect`: a `TEXT_EDIT_FRAME_PAD` (3px) gap on every side of
the *ink*, measured from the font's real glyph rows plus the drop-shadow row
rather than from the caller's line height — line height carries interline slack
that otherwise reads as dead space under the text. The wizard sizes its box to
the prompt, the popup spans the dialog's inner width.

A field opens with its whole seed text selected, matching DOS: `FUN_6f74_2580`'s
edit branch keys off bit `0x80` of the field record at dialog `+0x30`, which the
first keystroke clears by wiping the buffer (the record also holds the max length
at `+0x06` and a far pointer to the buffer at `+0x0c`).

**Behaviour deliberately diverges from DOS.** The original only supports append,
Backspace and that one select-all flag — there is no caret to move. Since this is
UI rather than simulation, the widget is a normal modern text box: arrow keys,
Ctrl+arrows for word jumps, Home/End, shift+any of those to extend a selection,
Delete, Ctrl+A/C/X/V, click and drag to place or drag-select. Enter and Esc are
handed back to the caller (`TEXT_EDIT_ACTION_CONFIRM` / `_CANCEL`) so confirm and
cancel keep their per-dialog meanings. The caret draws as DOS's trailing
underscore at the end of the text and as an insertion bar inside it.

Ctrl+C/X/V use the system clipboard: `src/main.c` installs
`platform_clipboard_get` / `platform_clipboard_set` as hooks via
`text_edit_set_clipboard`, and the widget falls back to a process-local buffer
when no hooks are set (tests, headless tools). Covered by
`tests/unit/test_text_edit.c`.

Enter or left-click **skips** the remaining sail frames (QoL; original is hard to skip). `AMERICA.MOV` is a short motion/script blob for map tooling, **not** the dock voyage cutscene.

### Map generation (NEW WORLD)

VICEROY (not MAPEDIT) builds random maps in `FUN_684c_08c0`
(`original_sources_decompiled/viceroy_unpacked.c`). The Linux port mirrors that
pipeline in `src/core/map_gen.c` into the same three-layer layout as `.MP` files
(terrain filled; layer2/3 left 0 for gen v1). Size is fixed **58×72** (`0x3a`×`0x48`).
CUSTOMIZE edits the four UI axes on `CUSTOMIZ.PIK` via `FUN_733a_0270` /
`NEW_GAME_PHASE_CUSTOMIZE` before the shared wizard.

Parameters (DOS words at `DS:0x1e7e`, each 0..3 from NEW WORLD `range(0,3)`; CUSTOMIZE UI keeps 0..2 via `% 3`):

| Index | UI (`LABELS.TXT`) | Role |
|------:|-------------------|------|
| 0 | Land Mass: Small / Moderate / Large | Land budget with form: `(form + mass + 1) * 0x140` |
| 1 | Land Form: Archipelago / Normal / Continents | Blob growth style |
| 2 | Temperature: Cool / Temperate / Warm | Latitude band shift |
| 3 | Climate: Arid / Normal / Wet | Humidity / river density |
| 4 | (internal) | Extra forest-pass count |

Pipeline: land-mask blobs → diagonal coast cleanup (masks 6/9) → latitude/temperature paint → climate humidity → forests / hills / mountains / rivers. Ocean / high seas indices **25 / 26**; feature bits `0x20` / `0x40` / `0x80` as above. Special resources stay draw-time procedural (not baked into layer2). CUSTOMIZE UI: `CUSTOMIZ.PIK` + `@MISC` labels; finished confirm is the bottom strip (`y > 184`).

RNG is the exact DOS libc LCG (`FUN_1d1d_0e04` / `FUN_19ef_0032` in `src/core/dos_rng.c`). NEW WORLD: seed → draw customize axes with **`range(0,3)`** → **reseed with the same tick** → `map_generate` (`FUN_684c_08c0`, ending in `LAB_684c_1b4c` HS-rim landfalls per nation). Tribe placement (`FUN_6a09_0006`) **reseeds to `rng_seed`** at entry (matches DOS `6a09` / VR_SEED timer word; not post-mapgen and not a post-axes restore). Golden check: `golden_mapgen_seed100` vs [`original_saves/mapgen/SEED100.SAV`](../original_saves/mapgen/SEED100.SAV) (axes `(0,0,0,2,2)` from seed 100). AI Europeans start in Europe harbor sentinels on NEW WORLD with landfall `goto` on the western rim of eastern high seas; Europe exit (`FUN_48d3_048e` / dispatcher) places near that goto — not southern ice from sentinel Y. AMERICA keeps on-map `@SCENARIO` fleets.

**Fixed seed (`--seed N`):** the port has one chokepoint for the DOS timer word (`game_pick_rng_seed` in `src/core/game_loop.c`, mirroring `FUN_281f_04ca` ← DS:0x83a6). `--seed N` (including `0`) overrides every timer-word read: NEW WORLD/CUSTOMIZE mapgen + `FUN_6a09` tribe reseed, Brave pulses, per-nation `ai_nation_reseed`, and the load-save LCG continuation. `--seed 100` reproduces the VR_SEED golden series, so DOS (VR_SEED patch) and the port can be run side by side to find divergence. Sound `pick_rng` is a fixed private constant (no game-state effect). Without `--seed` (and with `settings.json` `"seed": null`) the fallback is `elapsed_ms`.

| Extension | Typical use |
|-----------|-------------|
| `.PIK` | Packed pictures / backgrounds |
| `.SS` | Sprite sheets / animation frames |
| `.PAL` | VGA palette (`VICEROY.PAL`, 1024 bytes = 256×RGBA, 6-bit VGA RGB) |
| `.COL` | MicroProse sound drivers (`A/G/P/RSOUND.COL`) + `CONFIG.COL` |
| `.MOV` | Short motion / script tables (e.g. `AMERICA.MOV`; not the LEVN voyage) |
| `.MP` | Map data |
| `.DAT` | Tables / path data |
| `.BIN` | Large binary blobs (e.g. `COLDIG.BIN` digital SFX — decoded and played, see Music / sound) |

## Music / sound

There are **no** standalone `.MID` / `.XMI` song files. Music lives inside the MZ sound
drivers. The Linux port loads **`GSOUND.COL`** (General MIDI) and **emulates the driver
literally** in [`src/core/gsound_vm.c`](../src/core/gsound_vm.c); [`src/core/sound.c`](../src/core/sound.c)
ticks that VM in real time from the audio callback and mirrors the DOS BGM scheduler
(segment `129f`).

| Driver | Card letter | Role |
|--------|-------------|------|
| `ASOUND.COL` | A | AdLib / OPL |
| `GSOUND.COL` | G | General MIDI (**used**) |
| `PSOUND.COL` | P | PAS / SB-family |
| `RSOUND.COL` | R | Roland / MT-32-style |
| `CONFIG.COL` | — | 20-byte INSTALL card config |
| `COLDIG.BIN` | — | Digital SFX — 35 samples, **decoded and played** (2026-08-27) |

DOS play path: numeric sound IDs through the driver jump table (`FUN_2059_000a`), gated by
Background / Event / SFX (`FUN_12d8_000e`). IDs `0x20..0x3f` are background music;
`0x40..0x5c` event music; IDs `< 0x10` are system (0 = hard stop, 1 = fade out).

### Song ids (verified 2026-08-27)

The DOS Pick Music handler (`2b5a:264c` jump table + sublist offsets) and the tune table in
`FUN_129f_0008` give the **real** id ↔ title mapping. Earlier docs assumed entry *n* → `0x20+n`;
that was wrong for 5 of the 12 main tunes and every sublist, which is why the port sounded
like "a different song". Verified by chroma-DTW against a DOSBox-X capture (Jine the Cavalry =
`0x25`, DTW cost 0.04) and the OST rips in `reference_music/` (which match DOSBox exactly).

| Pick Music entry | id | | entry | id |
|---|---|---|---|---|
| 1 Bird Song | `0x20` | | 7 Joe Clark | `0x26` |
| 2 Smoky Tune | `0x21` | | 8 Little Fiddle | `0x27` |
| 3 Cornwall | `0x22` | | 9 Hornpipe | `0x39` |
| 4 Shady Grove | `0x23` | | 10 Bonny Morn | `0x38` |
| 5 Fiddler's Dance | `0x24` | | 11 Hole In The Wall | `0x3a` |
| 6 Jine the Cavalry | `0x25` | | 12 Nightingale | `0x3b` |

Independence `0x29..0x2d` (Love Forever … Independence Way), Military `0x2e..0x31`
(Reveille, Successful Campaign, Morelli's Lesson, To Arms), Indian `0x32,0x33,0x35,0x36`
(Indian Victory, Natives, Tenochtitlan, Pizarro at Cuzco). `0x28`, `0x34`, `0x37`, `0x3c..0x3f`
are not in Pick Music (`0x28` is drawn by the Europe pool; `0x3c` picks random patches in its
handler). The combat cue `0x32` is therefore **Indian Victory**. The title-screen id `0x33`
(= Natives) is inherited from older notes and unverified.

### DOS BGM scheduler (`FUN_129f_00f6` / `0318` / `02cc`)

* `DS:0x9a` is a **tune pool**, not a track: 1 = map (tunes 1–7: Bird Song … Bonny Morn,
  Hole, Nightingale), 2 = colony (8–12: Fiddler's, Jine, Joe Clark, Little Fiddle,
  Hornpipe), 3 = Europe (`0x28` + Independence), 4 = Military, 5/6/7 = one-shot Natives /
  Tenochtitlan / Pizarro then the general pool (all 12, 1-in-8 chance of the 13–23 set).
  `sound_set_bgm(pool)` mirrors `FUN_129f_0318`: a pool change fades the current song.
* All songs **end** (no `FD` loop except `0x34`); when the driver reports no voice active
  the pump draws the next random tune from the pool, never repeating the last id.
* `sound_play(id)` = `FUN_129f_02cc`: queue + fade, pump starts it when idle.
  Pick Music selection plays immediately and becomes the current BGM (`FUN_281f_04c0`).

### GSOUND driver facts (from `gsound.asm` / raw ndisasm of the MZ image)

* PIT divisor `DS:0081 = 0x4DBF` → **59.95 Hz** ticks; a note lasts exactly `dur` ticks
  (`SUB [bx],1 / JBE parse`); gate = `F6` abs or `dur − F7`; `F7 ≥ 0x80` ties repeated notes.
* Nine voice blocks (`DS:0x8096 + 0x28·i`) map to MIDI channels **1..9** in that order;
  the block at `0x80BE` is channel **9 (GM percussion)** — songs `0x27..0x31`, `0x36`, `0x3d`
  put their drum track there (`call 0x15e0`). Melodic tracks take the first free of
  channels 1–6 (`0x14cd`); event music uses 7–8 (`0x15c1`).
* Song handlers are x86 stubs (`call 0x18cf` fade-old-voices, `mov cx,stream / call alloc`);
  some use the PRNG (`0x3c`), warm-restart variants via `DS:E6/E8/EA` (`0x25`, `0x34`) or
  `C4` code call-outs (`0x20` pokes note bytes). `gsound_vm.c` runs them with a mini x86.
* Opcodes: `≤BA note,dur`; `ED n notes dur` chord; `F4` vel; `F8` prog; `F0/F1/F2` pan/vol/bend;
  `F3/F5/EF` vol/pitch/pan envelopes; `BB n` RPN bend range; `C0/C1/C2` bank/chorus/reverb;
  `C5..CC` conditional **call** (single return slot `+0x22`), `CD..D4` conditional **jump**;
  `D5..E9` byte ALU on `DS:0x5C+r`; `E7/EA/EB/EC` self-modify the stream; `FA/F9` call/ret
  (one slot, not a stack); `FB/FC/FD` jump / set loop / loop-to-start; `FE/FF` counted loops.
  `BE/BF/BC/BD` write tempo words nobody reads — timing is the PIT only.
* Song switch: old voices get `+0x26 = 0xFF` and fade 2 CC7 units/tick (`0x1819`) while the
  new song starts on free voices — a real cross-fade, reproduced.

MicroProse GM drivers of this era were written for **Roland Sound Canvas / SC-55**.
Closest practical playback: FluidSynth + an SC-55-character SoundFont.

**SoundFont search order:** `COLONIZE_SOUNDFONT` → bundled
[`data/soundfonts/Roland_SC-55.sf2`](../data/soundfonts/Roland_SC-55.sf2) (ScummVM’s
GPL-3+ bank by deemster; see [`COPYRIGHT.Roland_SC-55`](../data/soundfonts/COPYRIGHT.Roland_SC-55))
→ system SC-55 / GeneralUser GS → FluidR3 / distro defaults. For an alternate SC-55
character, point `COLONIZE_SOUNDFONT` at [Trevor0402’s SC-55 SoundFont](https://github.com/trevor0402/SC55Soundfont).
Gold A/B reference: `original_music_dumps/jine_the_cavalry.wav` (DOSBox-X capture) and
`reference_music/wav/*` (OST rips, verified identical to DOSBox timing). Run
`build/dump_gsound_wav --ab && .venv-sound/bin/python3 tools/compare_music_ab.py`
(2026-08-27: dtw 0.04 / 0.09 / 0.15 / 0.17, drift ≤ 1.1 s). `build/dump_gsound_wav --midi`
writes every BGM id to `ripped_sound/` as WAV + Type-0 MIDI.
AdLib / MT-32 drivers remain out of scope. `COLDIG.BIN` SFX are in (see below).

Song names for the Pick Music UI are only in `GAME.TXT` `@PICKMUSIC` (plus Independence /
Military / Indian sublists). Options are `@SOUNDOPTIONS` and Col1 `tut2` bits.

**Pick Music (GAME menu):** implemented in [`src/core/pick_music.c`](../src/core/pick_music.c) as a
shared wood **popup** (`popup_draw` + `WOODTILE.SS`) over the map, using the id table above.
Selecting a song plays it at once as the current BGM (closing the dialog does not stop it).

### Sound-ID ranges beyond the 12 BGM tracks (RE notes)

`FUN_1000_19bc` (the driver's numeric-id dispatcher, `GSOUND.COL` and `PSOUND.COL` alike)
has four ranges, all traced by disassembling the raw `.COL` MZ images (not just the
overlay-affected `VICEROY.EXE` decompile):

| Range | Table (image offset) | Confirmed behavior |
|-------|----------------------|---------------------|
| `< 0x10` (only 9 entries, ids 0–8) | `0x2A5C` | **Channel reset/silence**, not player-audible content — e.g. id 4's handler resets MIDI channels 6–7 (`CC121`/`CC123` all-notes-off + reset-controllers), id 1's handler mutes two specific voice slots. `sound_play` already treats id 0/1 as "stop" (`src/core/sound.c`). |
| `0x20..0x3f` BGM | `0x2A6E` | The 12 Pick-Music tracks + named submenus (Independence/Military/Indian) **and** situational ids outside those submenus (`0x24`, `0x25`, `0x3e`) pushed directly by DOS gameplay code via `FUN_281f_048e`→`FUN_129f_02cc`. Confirmed real trigger: combat (`FUN_5fef`, land+naval) pushes `0x32` ("Military" sublist track 1) when an engagement begins — ported as `units_combat_music_sting()` (`units.c`), gated through `units_set_combat_music_hooks` (kept as a function-pointer hook so `units.c` stays linkable without `sound.c` in standalone `unit_*` test binaries). Other confirmed-real-but-unmapped-to-a-precise-trigger call sites: segments `65dd` (LCR), `75c2` (save/load), `48d3` (Europe exit), `364b` (colony), `38fd`/`3844` (trade) — left unwired pending closer per-site tracing. |
| `0x40..0x5c` "event music" | `0x2AC4` | **Triggers found 2026-08-27** — pushed with the id in **AX** (`mov ax,N; callf FUN_281f_04c0`), which Ghidra drops from the decompile, so the earlier "no confirmed trigger" verdict was a decompiler artifact. Each handler queues a `COLDIG.BIN` sample and starts a short MIDI sting on channels 7/8. Decode + playback + mixing are done; per-id push sites and their port wiring status are in the "COLDIG.BIN" table below. |
| `≥0x8020` (7 entries, ids `0x8020..0x8026`) | `0x2AB6` | Short pre-scripted multi-voice MIDI chord stings (writes directly into the same voice-struct engine used for BGM playback — not digital audio). No confirmed DOS caller found (the one literal `0x8025` reference elsewhere in `VICEROY.EXE` turned out to be an unrelated dialog-box parameter, not a sound id). Left unwired. |

The `sound_effects` option flag (`ColonizeSoundOptions.sound_effects`, DS offset `0xa2` in
the driver) is real and consulted by DOS at several BGM-change call sites
(`FUN_129f_0300`/`0318`/`034c`) — but it gates **whether a BGM track change applies
immediately or gets deferred to the next idle-pump poll**, not a separate audio category.
`sound_play`'s existing BGM gating (`background_music`/`event_music` bits) already covers
the player-visible effect; the immediate-vs-deferred nuance is DOS-internal scheduling with
no equivalent complexity in the port's single-threaded playback and was not replicated.

**`COLDIG.BIN` digital SFX — wired 2026-08-27.** Earlier notes ("no reachable trigger,
settled negative") were wrong: the game pushes event ids `0x40..0x5c` with the id in **AX**
(`mov ax,N; callf FUN_281f_04c0` → `FUN_12d8_000e`), which Ghidra's decompile drops. Each
event handler in `GSOUND.COL` does `mov ax,N; call 0x30c4` → `FUN_1000_27b4(N)` (queue COLDIG
sample N, 16-slot ring, played back to back) and then starts a short MIDI sting on channels
7/8. Sample table is static in the driver image at `0x1C7B` (`offset32,len32`, 35 entries,
exactly covering the 993 755-byte file); samples 0–4 play at 11025 Hz, 5–34 at 19050 Hz,
unsigned 8-bit. Stream opcode `C3 n` is the same trigger. `sound.c` loads the file, the VM
callback queues samples, and `sound_render_s16` mixes them after the synth. Dump with
`build/dump_gsound_wav --sfx` → `ripped_sound/sfx/sfxNN.wav`.

Sample contents (user listen test 2026-08-27): 0–4 single shots/fireworks; 5–7 screaming +
shots; 8, 19 burning; 9 cheering; 10, 15 cheering + fireworks; 11 screaming + shooting;
12 wagon wheels; 13 hammering then cheering; 14 shooting + galloping; 16 sinking; 17 shot
glancing; 18 shot; 20 animal shot; 21 pump-action; 22 gunfight; 23–34 shots (25–26 cannon).

| Event id | COLDIG | Sound | DOS push site | Port |
|---|---|---|---|---|
| `0x40`/`0x41` | 31 / 32 | shot | `5fef_1b0e` attack fire (0x41 artillery class), **only when `param_4` (visible) is set** — `465b_0000` passes 1 for the viewport nation or a human side, the AI scorer `521d:52aa` passes 0 | `units.c` engagement (0x40), gated by `units_combat_is_visible` (2026-08-28 — AI-vs-AI combat was audible, cannon fire landed over unrelated popups) |
| `0x42`/`0x48` | 30 / 29 | shots | `5fef_1b0e` 5fef:2271: human attacker vs Indian (nation ≥ 4) pushes `0x3b + attacker unit type` — Cont. Cav. (7) / Treasure (0xd) | `units.c` engagement (2026-08-29): typed id when defender is Indian |
| `0x43`/`0x49` | 27 / 34 | shots | same rule: Cavalry (8) / Wagon Train (0xc)… — the "unit-class variants" are the attacker's type index | same |
| `0x44`/`0x45` | 18 / 17 | shot / glancing shot | `5fef_1b0e` 5fef:28b0 tail, gated on `local_6` (set at 5fef:2546: attacker nation ≥ 4, a colony at the defender tile — `281f_07be(x,y)` ≥ 0 — and colony pop > 1 or `local_70 == 0`) + attacker won + visible → `0x44` if the *attacker* is a ship type (`local_86` = `Stack[4]` type in 0xd..0x12, unreachable for Indians) else `0x45`; `0x44` is also Cont. Army (9) via the typed rule | **2026-08-29**: `units_try_move` combat branch — Indian attacker beats a colony defender (colony at dest, pop > 1) → `0x45` after the 0x4a win beat; the ship-attacker `0x44` arm is dead |
| `0x4a`/`0x4b` | 28 / 33 | shots | `5fef_1b0e` win; 0x4b when natives involved | `units.c` win (same visibility gate) |
| `0x4c` | 14 | shooting + galloping | `0x3b + type` would need attacker type 0x11 (Frigate) — ships cannot attack land units, so this id is unreachable through the typed rule; no other push site located | — |
| `0x4d` | 10 | cheering + fireworks | `5fef_0352` 5fef:07db-0803: junction reached from 061c (winner is a ship), 0631, 0722 and the `@ARTILLERY2` popup; when *both* combatants are ship types (0xd..0x12) and the fight is visible → `0x4d`, **before** the damage-flag (5fef:0d0x `0x3148\|0x80`) / sink (`0x57`) / seizure split — a naval-win beat, not a capture; also raid loot | raid loot gold (`ai_contact.c` @RAIDGOLD); **2026-08-29**: `units_apply_naval_loss_outcome` entry (visible) → `0x4d`, ahead of damaged/sunk |
| `0x4e` | 6 | screaming | `5fef_0f14` raid: colonists killed | @RAIDSCALP (2026-08-29) |
| `0x4f` | 11+32 | screaming + shooting | `5fef_0f14` raid loot goods | @RAIDSTORES (2026-08-29) |
| `0x50`/`0x51` | 7+8 / 5+14 | screaming, burning / screaming, galloping | typed rule: Mounted Braves (0x15) / Mounted Warriors (0x16) — but the rule is gated on a *human* attacker, so these fire only for captured/converted native types | typed rule |
| `0x52` | 12 | wagon wheels | `465b_0000` wagon-train move (human) | `game_loop.c` human move success, type "Wagon Train" (2026-08-29) |
| `0x53` | 19 | burning | `5fef_0f14`/`1b0e` tail: colony burned | colony burned notify |
| `0x54` | 13 | hammering + cheering | found colony `479b_076e`; colony screen `2f2b_6cd4` **only when `DS:0x34a >= 0`** (the building that just finished, revealed by clear-bit/redraw/set-bit/redraw); nation EOT `3844` | found colony; colony open **gated** on `ColonizeColony.pending_build_reveal` (2026-08-28 — was every open) |
| `0x55` | 20 | animal shot | `0x3b + type` would need type 0x1a (past the land-unit range) — unreachable through the typed rule; no other push site located | — |
| `0x56` | 9 | cheering | `38fd_3dc8` tax raise / tea party | `ai_king.c` @TEAPARTY + raise-taxes popup (2026-08-29) |
| `0x57` | 16 | sinking | `5fef_0352` ship sunk | `units.c` @SHIPSUNK via the combat sound hook (2026-08-29) |
| `0x58` | 21 | pump-action | fortify / sentry (`2b5a_1112`, `2f2b_5746`) | fortify, sentry |
| `0x5a` | 15 | cheering + fireworks | `5fef_1908` King's Galleon (via `FUN_281f_04b6`) | galleon credit |
| `0x5b` | 22+31 | gunfight | `5fef_0f14` raid repelled | @RAIDNOTHING (2026-08-29) |
| `0x5c` | 8 | burning | typed rule: type 0x21 (past the unit table — unreachable) | — |
| `0x8020` / `0x8024` | — (chord stings) | | war declaration `5bfb_153e`, assign colonist `2f2b_2f3e` | **wired 2026-08-29**: `FUN_1000_19bc` has a fourth handler table at `0x2AB6` indexed `id − 0x8020` (bound `DS:0xFE`); `gsound_vm.c` now dispatches it, gated by Event Music. `ai_diplo_declare_war_ctx` (human involved, via `ai_diplo_set_sound_hook`) and the three colony-screen assign sites |

**BGM cues pushed by gameplay code (2026-08-29 asm sweep of every `281f_04c0`/`04b6`
call):** `75c2_235c` new-game init → `0x39` Hornpipe once (ported: game_loop new-game start);
`38fd_3dc8` King's audience → `0x3e` (ported: `ai_king.c` audience CHOICE); `43f7_10f0`
intervention → `0x3f` after `@INTERVENE` (ported); `41f2_0b70` Retire → `0x24`/`0x25`/`0x21`
by the coin-animation tier (≥23 / >6 / else) — not ported (tier derivation still PARK, see
difficulty.md); `364b_0000` is **not** a colony-screen open — it is the colony-screen popup helper
(tags NOMOREWAREHOUSE/NOMOREWAGONS/BUILT/DEPLETION/REBEL*/TORY*/SONSDOWN via
`thunk_FUN_291f_09dc`, 9 sites all inside `364b`) whose 7th arg (`Stack[0x10]`) is an
optional event id — **every caller passes 0** (the NOMOREWAREHOUSE site pushes `AX`,
which is 0 on that path), so no colony popup carries a sound (resolved 2026-08-29);
`2b5a_2464` Pick Music. Pool switches
`281f_04b6(n)` → `129f_034c` (`DS:0x9a = n`, restart if changed): `1` map at colony EOT
(`364b_0688`, when `0xa897`) and for a human *loser* of a naval fight (`5fef_0352`), `4`
Military for a human naval *winner*, `2` colony pool on building complete (`364b_0114`) and
after the King's Galleon popup (`5fef_1908`). Also (`281f_048e` = queue-next, `281f_0498` = pool switch, 2026-08-29 sweep): `48d3_06ba`
treasure cash-in → `0x24` (ported: `europe_cash_treasure` via `europe_set_sound_hook`);
`75c2_20e2` load → `0x3e` (ported); `75c2_2778` Europe screen → pool 3 (ported at both
Europe-open sites); `38fd_5e52` human immigrant → pool 2 (ported, `europe_notify_immigrant_sound`);
`65dd_0004` LCR: Fountain of Youth → `0x37`, Cibola → `0x3c`, Burial Mounds → `0x33` (ported in the
`units.c` LCR switch), plus (mapped 2026-08-29, `units_resolve_lcr_rumour`): case 3/7 small treasure / chief's gift
with gold → pool 2 (`65dd:04ca`); case 5 vanish → pool 1 (`65dd:0778`); burial-mounds
BURIAL3 treasure with no tribe claim → `0x24` (`65dd:0654`), a claim → `0x32` ahead of @SCREWED
(`65dd:06e6`) — all human-only (`local_a`); `4d56_2820` village visit (human, 1-in-3 roll) → pool 5, Inca 7, Aztec 6
(ported in `ai_contact_speak_with_chief`); `5bfb_022e` first meet → same pools from turn 20 (`04ac` = `129f_0318` restart-if-changed, `0498` = the option-gated wrapper; ported in `ai_contact_enqueue_welcome`);
`43f7_1d42` after `@KINGBUY` → pool 3 (KINGBUY itself unported); `43f7_10f0` → pool 3 then `0x3f`
(ported); `3844_00f2` nation EOT → `0x3e` ahead of **`@KINGFRIGATE`** (`LEA BX,[0xef5]`; Crown offers a Frigate to a
harassed, frigate-less nation every 8th peacetime turn — ported 2026-08-29 as `ai_king_frigate_offer`, tune
included);
`5fef_0f14` raid → pool 2 when the raid is wiped out (`local_6 == 0`, 5fef:1299) / `0x32` for any
other outcome (5fef:13b2) — both already in `ai_contact.c`'s raid tail; `41f2_0b70` Retire → `0x24/0x25/0x21` by coin
tier (PARK). The port's `sound_set_bgm(1/2)` covers the map/colony switches; the naval `1`/`4`
beat is wired too (`units_set_bgm_hook`: human loser → pool 1, human winner → pool 4); a wiped-out raid on a human colony → pool 2 (`ai_contact.c` @RAIDNOTHING).

Event ids bypass the BGM scheduler (`sound_play` dispatches them directly), gated by the
Event Music option in the driver and by Sound Effects for the PCM part.

## Discovery Order

Intended install layout: put original game files in `<executable-dir>/COLONIZE/`.

1. Explicit `--data-dir` (if that path exists)
2. `<executable-dir>/COLONIZE`
3. `./COLONIZE` (working directory)
