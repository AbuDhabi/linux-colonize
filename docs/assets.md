# Colonization Asset Notes

For a navigable index of decomp sources, `COLONIZE/` data files, and DOSBox memory
dumps, see [original_index.md](original_index.md).

Original game data (required to play) belongs in a `COLONIZE/` directory next to
the executable — the same layout as a DOS install. The port creates that folder
empty on startup if it is missing; copy the shipped MicroProse files into it.
Override the search root with `--data-dir`. Saves default to the same directory
(see [savegame.md](savegame.md); override with `--save-dir`).

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

Content draws inside the rect inset by `POPUP_FRAME_INSET` (3). Call `popup_draw(...)`; on palettes that differ from WOODPANL (e.g. `OPENMENU.PIK`), use `popup_colors_from_ui` + `popup_colors_remap`. First consumer: title `@BEGINMENU`. Not the same as `WOODFRAM.SS` (colony frame graphic) or map-menu pulldowns. Which game dialogs use this chrome (and port status): [popups.md](popups.md).

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

**Layer 3**: fog/visibility at runtime. AMER2 also uses **`0x0e`** at `(43,68)` as an isolated mountain peak marker (PHYS0 **32**).

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
| 148 | (reserved / solid) |
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

Layer-3 `0x0e` on AMER2 `(43,68)` is a lone tundra peak drawn as isolated mountain **32** (no hill bit in the terrain byte).

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

On the main map, the top strip is the DOS menu bar from `MENU.TXT`: **GAME**, **VIEW**, **ORDERS**, **REPORTS**, **TRADE**, **COLONIZOPEDIA**, with fixed slots for **CHEAT** (`@CUP`) and (when built with `COLONIZE_DEBUG_MENU`, default ON) **DEBUG** between TRADE and COLONIZOPEDIA. The bar uses `WOODTILE.SS` fill (same as the right panel), `FONTTINY.FF`, `@COLORS` basic green with yellow (`~`) hotkeys, and a 1px black rule across the full width. Open pull-downs use the same wood fill (screen-aligned grain), a 1px black outline, green item text with yellow hotkeys, and sit **1px below** the bar rule so map pixels show in the gap. **Separators** are not in `MENU.TXT` (except PEDIA’s cosmetic `---`); DOS `FUN_74a4_0000` inserts empty-label rules into GAME / VIEW / ORDERS / REPORTS / CHEAT / PEDIA — the port mirrors those. ORDERS hide/gray follows `FUN_2b5a_0b34` (Move Pieces) / `0902` (View Pieces). Below the bar the screen splits into a **15×12-tile map viewport** (`x=0..239`, `MAP_PANEL_X` / `MAP_VIEW_*` in `src/core/map_panel.h`) and a **right info panel** (`x=240..319`, 80px) with a 1px black left edge. The world map is stored at 58×72 including a 1-tile rim; the viewport origin is inset by one tile (`FUN_6ba1_000c` / `map_panel_clamp_view_origin`) so the rim is never scrolled onto the main map (visible area 56×70). Click a title to open its pull-down; click an item to activate it (grayed items are stubs). Esc closes an open menu; Esc with no menu open returns to the title screen. Left-click on the map viewport: select an owned unit with moves, else select the tile (owned unit with no moves, empty land, etc.), or open an owned colony. Right-click always selects the tile (and clears unit selection). While a unit is selected, left-click only pans the viewport. Left-click on the panel minimap centers the view on that world tile. The selected tile shows a blinking white outline; over the map viewport the OS pointer uses `CURSOR.SS` #0.

**CHEAT:** Hidden until unlocked. On the main map, press **Alt-W**, **Alt-I**, **Alt-N** in succession (spells WIN); press **Alt-W** again to hide. Layout still reserves the CHEAT title slot while hidden so DEBUG does not shift. Enabled items: **Reveal Map** (Shift-F4) opens `DEBUG.TXT` `@SETVIEW` — English/French/Spanish/Dutch/Complete Map / No Special View. This is a reversible fog viewpoint (does not rewrite `seen[]`); Complete Map sets Col1 `show_entire_map`. **Kill Indians** (Shift-F6) picks a native nation (4–11) and removes its villages and units. Other CHEAT entries remain grayed.

**DEBUG** (CMake `COLONIZE_DEBUG_MENU`): **Sprite Viewer** (same as `` ` ``) and **Show Mouse Coords** (toggles the pixel HUD attached to the pointer; on by default).

Working items today: Save/Load, Retire, Exit, **Pick Music**, European Status, Find Colony, Center View, Activate unit, Wait for next unit, Fortify (land/ship harbor) / Sentry / Disband, Clear Forest↔Plow / Build Road, Go to Place↔Port, Pillage, Dump Cargo Overboard, Begin Trade Route (aim first stop + cycle; Col1 load/unload nibbles honored when set), TRADE Create/Delete / Edit (append colony or Europe stop; autofill + thin unload/load cargo picker), Build/Join Colony, Load/Unload Cargo (board/unload), Return to Europe, No Orders (end turn), full **COLONIZOPEDIA** menu (cargo / units / terrain / skills / buildings / fathers / misc; divider after terrain), **F1** terrain info at cursor, and **REPORTS** F2–F10. ORDERS plain-key hotkeys + Alt+menu titles.

### Main-map right panel

DOS layout: 15×12 main view (see `MENU.TXT` zoom levels) leaves an 80px strip. Implemented in `src/core/map_panel.c`:

| Element | Asset / source | Notes |
|---------|----------------|-------|
| Wood fill | `WOODTILE.SS` | Tiled over `x=240..319`, below the menu bar (and the menu bar itself) |
| Left rule | 1px black | `x=240`, full panel height |
| Minimap section | wood + 1px black separator below | Section is larger than the minimap; wood shows in the margins |
| Minimap | 56×39 window (1px/tile) | AMER2 visible interior (not full map / not rim); scrolls with the main view; origin inset like DOS; dark-orange border (palette 6) flush to section black rules; terrain/ocean/high-seas, colony (white) / tribe (palette 12) / unit dots; white view outline on edge tiles; click centers main view |
| Unit block | `NAMEPLAT.SS`, `ICONS.SS`, `LABELS.TXT` `@INFO` | Portrait + name; `Moves:` / `Locat:` / `With:` hold icons (passengers + goods; empty recessed slots up to `@UNIT` cargo) |
| Date + gold + tax | Campaign calendar + `EuropeScreen` gold/tax | `Spring 1492` / `Gold: N$  Tax: N%` (`FONTTINY.FF`, `@COLORS` basic) |
| Tile details | `NAMES.TXT` / WorldMap `improve` (Col1 mask fallback) / colonies / units | Under Locat: ownership, `(Terrain)`, features (plow/road/river/resource/rumour), colony or native camp (`ICONS.SS` **#0–3** / **#10–13**) + units with order label |
| Nation box | Turn indicator at `(315,197)` | Existing 5×3 owner color; kept in the panel strip |

Not used for this panel: `WOODPAN2.PIK` (score/fame chrome), `WOODFRAM.SS` (colony frame). Fog-of-war on the minimap is not drawn yet.

### Report / adviser screens

Open from **REPORTS** on the map menu bar, or press **F2–F10**. Esc (or Enter) returns to the map. **F1 Terrain Information** opens the Colonizopedia entry for the terrain under the map cursor (not a report plate). Backgrounds:

| Key | Report | Background |
|-----|--------|------------|
| F1 | Terrain Information | Colonizopedia (cursor tile) |
| F2 | Religious Adviser | `REPORT2.PIK` |
| F3 | Continental Congress | `CCBKGD.PIK` |
| F4 | Labor Adviser | `REPORT4.PIK` |
| F5 | Economic Adviser | `REPORT5.PIK` |
| F6 | Colony Adviser | `REPORT6.PIK` |
| F7 | Naval Adviser | `REPORT7.PIK` |
| F8 | Foreign Affairs Advisor | `REPORT8.PIK` |
| F9 | Indian Adviser | `REPORT9.PIK` |
| F10 | Colonization Score | `WOODPANL.PIK` (full-screen wood) |

Content uses `ColonizeCol1Save` when a campaign is loaded (crosses / founding fathers / tribes / trade ledger / rival strength), with runtime colony / unit / Europe pools as fallback. **F10** uses the manual score schedule (citizen quality, congress, gold/1000, rebel sentiment, village-burn penalty, independence multipliers when declare/achieve are tracked).

### Colonizopedia

Open from **COLONIZOPEDIA** on the map menu bar, or press **P** (Cargo Types list). Each category opens an encyclopedia **list** on `WOODPANL.PIK`: white **ENCYCLOPEDIA OF COLONIZATION** header, green clickable entry titles in up to three columns (`FONTTINY.FF`), and **(Exit)** top-right. Click an entry for the article; Esc returns to the list; Esc / (Exit) / P from the list returns to the map. In an article, Left/Right (or Up/Down) cycles entries.

The pull-down has a horizontal green rule between Terrain Types and Colonist Skills (DOS `FUN_74a4_0000` empty-label sep after Terrain; `MENU.TXT` `---` is ignored on load and the same sep is inserted in code).

| Menu item | PEDIA.TXT | Preview |
|-----------|-----------|---------|
| Cargo Types | `@CARGO0`–`15` | `ICONS.SS` #22–37 |
| Unit Types | `@UNIT0`–`23` | `ICONS.SS` (`NAMES.TXT` `@UNIT` icon) |
| Terrain Types | `@TERRAIN0`–`28` | 3×3 TERRAIN/PHYS0 composite |
| Colonist Skills | `@JOB0`–`27` | related cargo/unit icon |
| Colony Buildings | `@BUILDING0`–`41` | `BUILDING.SS` |
| Founding Fathers | `@FATHER0`–`24` | `CC-00.SS`–`CC-24.SS` |
| Miscellaneous | `@MISCELLANEOUS` titles | text blurbs (no PEDIA bodies in data) |

**F1** / **REPORTS → Terrain Information** jumps straight to the terrain article for the cursor tile (Esc exits to the map).

### Europe (home port) bring-up

Press **E** from the map to open the European Status screen (`EUROPE.PIK`). Esc or E returns to the map (closes open menus first). Visual layout reference: [`original_screenshots/europe/`](../original_screenshots/europe/). Harbor / Expected / Bound ships and dock immigrants draw orders/allegiance chrome (`unit_chrome.c`) behind their `ICONS.SS` sprites.

Transit (manual 1–4 turns; port interim **east 2 / west 4**, −1 if ship moves ≥6 — **Unverified vs DOS**):

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

`CLOS-BKG.PIK` is the independence closing-sequence backdrop — not used here.

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
| H | Sail selected ship to Europe (must be on a high-seas tile, terrain index 26); passengers stay aboard in harbor |
| P | Pioneer selected with moves: plow/clear (20 tools). Otherwise open Colonizopedia |
| R | Pioneer selected with moves: build road (20 tools). Otherwise unused on map (Europe: recruit) |
| B | Found a colony on the cursor tile: land unit is disbanded into a colony colonist; tools/muskets/horses go to the warehouse stub (ships cannot found) |
| Space | End turn (`src/core/turn.c`): advance calendar (`@TIMECHANGE`), run colony production + nation ticks, Euro/Indian AI + King/REF, refresh human movement |
| ORDERS → Wait | Select next human unit with remaining moves (“Continue turn.”); if none and End of Turn option is set, show “End of Turn” |

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
4. Difficulty (`DIFFICUL.PIK` image regions + 1px border, click to select / Enter or finished to confirm) → nation (`NATIONS.PIK`, same) → leader name on `WOODPANL.PIK` (unbold `FONTINTR` + green+shadow text / green input box) → `@NATION{n}A` / `B` on wood (same unbold `FONTINTR`, green+shadow; body lines flow-wrap to `@width` like DOS `FUN_6f74_1198`, vertically centered) → king audience → `LEVN0001`–`0010.PIK` + `@BUILD1`–`10` yellow captions → map

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

RNG is the exact DOS libc LCG (`FUN_1d1d_0e04` / `FUN_19ef_0032` in `src/core/dos_rng.c`). NEW WORLD: seed → draw customize axes with **`range(0,3)`** → **reseed with the same tick** → `map_generate` (`FUN_684c_08c0`, ending in `LAB_684c_1b4c` HS-rim landfalls per nation). Tribe placement (`FUN_6a09_0006`) **reseeds to `rng_seed`** at entry (matches DOS `6a09` / VR_SEED timer word; not post-mapgen and not a post-axes restore). Golden check: `golden_mapgen_seed100` vs [`test-saves-mapgen/SEED100.SAV`](../test-saves-mapgen/SEED100.SAV) (axes `(0,0,0,2,2)` from seed 100). AI Europeans start in Europe harbor sentinels on NEW WORLD with landfall `goto` on the western rim of eastern high seas; Europe exit (`FUN_48d3_048e` / dispatcher) places near that goto — not southern ice from sentinel Y. AMERICA keeps on-map `@SCENARIO` fleets.

| Extension | Typical use |
|-----------|-------------|
| `.PIK` | Packed pictures / backgrounds |
| `.SS` | Sprite sheets / animation frames |
| `.PAL` | VGA palette (`VICEROY.PAL`, 1024 bytes = 256×RGBA, 6-bit VGA RGB) |
| `.COL` | MicroProse sound drivers (`A/G/P/RSOUND.COL`) + `CONFIG.COL` |
| `.MOV` | Short motion / script tables (e.g. `AMERICA.MOV`; not the LEVN voyage) |
| `.MP` | Map data |
| `.DAT` | Tables / path data |
| `.BIN` | Large binary blobs (e.g. `COLDIG.BIN` digital SFX — not yet played) |

## Music / sound

There are **no** standalone `.MID` / `.XMI` song files. Music lives inside the MZ sound
drivers. The Linux port loads **`GSOUND.COL`** (General MIDI) via [`src/core/sound.c`](../src/core/sound.c):

| Driver | Card letter | Role |
|--------|-------------|------|
| `ASOUND.COL` | A | AdLib / OPL |
| `GSOUND.COL` | G | General MIDI (**used**) |
| `PSOUND.COL` | P | PAS / SB-family |
| `RSOUND.COL` | R | Roland / MT-32-style |
| `CONFIG.COL` | — | 20-byte INSTALL card config |
| `COLDIG.BIN` | — | Digital SFX (deferred) |

DOS play path: numeric sound IDs through the driver jump table (`FUN_2059_000a`), gated by
Background / Event / SFX (`FUN_12d8_000e`). IDs `0x20..0x3f` are background music;
`0x40..0x5c` event music; IDs `< 0x10` are system (stop). Title intro uses **`0x33`**.
Map BGM track *n* maps to ID `0x20+n` (track 1 → `0x21`).

### GSOUND stream format (RE from `GSOUND.COL` MZ)

Banner string: `Coloniz GMID09-12-94`. Voice bytecode is interpreted by a jump table at
image offset `0xEF2` for opcodes `0xBB..0xFF`. Timing uses PIT divisor **`0x4DBF`**
(~**60 Hz** ticks). Summary:

| Bytes | Meaning |
|-------|---------|
| `note, dur` with `note ≤ 0xBA` | Note or rest (`note==0`); duration in ticks |
| `ED n note×n dur` | Chord (≤4 notes); same gate/dur |
| `F4 vv` | Set velocity for following notes |
| `F8 pp` | Program change (GM patch) |
| `F1 vv` | CC 7 volume |
| `F0 vv` | CC 10 pan |
| `F3 period delta` | Volume envelope (per-tick CC7 ramp) |
| `BB n` | RPN pitch-bend range (CC101=0, CC100=0, CC6=n) |
| `C2 vv` | CC 91 reverb (**not** program change) |
| `C1 vv` | CC 93 chorus |
| `F6` / `F7` | Gate / articulation |
| `FA addr` / `F9` | Call / return (DS-relative) |
| `FF nn` | Loop (`nn==0` sets label; else repeat) |
| `BE a b` / `BF n` | Writes unread tempo product (IRQ still ~60 Hz) |
| `C4`..`EB` | Song ALU / conditional jumps (must skip correct sizes) |

Event music ids `0x40..0x5C` use table `0x2AC4` (`FUN_1000_19bc`). Interpreter notes:
[`original_sources_annotated/sound/gsound_interpreter.md`](../original_sources_annotated/sound/gsound_interpreter.md).

MicroProse GM drivers of this era were written for **Roland Sound Canvas / SC-55**.
Closest practical playback: FluidSynth + an SC-55-character SoundFont.

**SoundFont search order:** `COLONIZE_SOUNDFONT` → bundled
[`data/soundfonts/Roland_SC-55.sf2`](../data/soundfonts/Roland_SC-55.sf2) (ScummVM’s
GPL-3+ bank by deemster; see [`COPYRIGHT.Roland_SC-55`](../data/soundfonts/COPYRIGHT.Roland_SC-55))
→ system SC-55 / GeneralUser GS → FluidR3 / distro defaults. For an alternate SC-55
character, point `COLONIZE_SOUNDFONT` at [Trevor0402’s SC-55 SoundFont](https://github.com/trevor0402/SC55Soundfont).
Gold A/B reference: DOSBox Staging `mididevice=soundcanvas` (Nuked SC-55 + user ROMs).
AdLib / MT-32 drivers and `COLDIG.BIN` SFX remain out of scope.

Song names for the Pick Music UI are only in `GAME.TXT` `@PICKMUSIC` (plus Independence /
Military / Indian sublists). Options are `@SOUNDOPTIONS` and Col1 `tut2` bits.

**Pick Music (GAME menu):** implemented in [`src/core/pick_music.c`](../src/core/pick_music.c) as a
shared wood **popup** (`popup_draw` + `WOODTILE.SS`) over the map. Main-list songs map to BGM
tracks **1–12** (ids `0x21..0x2c`) in `@PICKMUSIC` order (Bird Song … Nightingale). Submenu
ids continue through remaining BGM slots, skipping Introduction **`0x33`**: Independence
`0x2d..0x31`, Military `0x32`/`0x34..0x36`, Indian `0x37..0x3a`. Selecting a song calls
`sound_play_preview` and updates the status line. Esc / click-outside stops the preview.

Title/map BGM via `sound_play` / `sound_set_bgm` is enabled (`COLONIZE_SOUND_PLAYBACK_ENABLED`).
`--nosound` skips the SDL device (and Pick Music previews). `smoke_sound` /
`smoke_pick_music` cover load + dialog + golden decode checks.

## Discovery Order

Intended install layout: put original game files in `<executable-dir>/COLONIZE/`.

1. Explicit `--data-dir` (if that path exists)
2. `<executable-dir>/COLONIZE`
3. `./COLONIZE` (working directory)
