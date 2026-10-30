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

The port decompresses MADSPACK/FAB and blits `.PIK` images. The main menu uses `OPENMENU.PIK` with its embedded palette, then overlays `@BEGINMENU` as a shared **popup window** (`OPENTILE.SS` fill, 3px wood bevel chrome from `src/core/popup.c`, `@width`/`@y`/`@smallfont` → `FONTTINY.FF`). Text colors come from `NAMES.TXT` `@COLORS` (`basic=68`, `{hilite}=149`, `select=138` on WOODPANL / in-game palettes; remapped by RGB onto `OPENMENU.PIK` so the title menu matches Colonizopedia greens — see `src/core/ui_colors.h`). Border colors (`border0/1/2`) are remapped the same way. Version line is `{COLONIZATION} OpenCol Port` + `COLONIZE_VERSION_STRING`. `CCBKGD.PIK` is the Continental Congress / Founding Fathers background.

### Popup window

Colonization uses the same wood dialog chrome for many confirmations and prompts. The port exposes it as a reusable template in `src/core/popup.c` / `popup.h`:

| Layer (outside → in) | Size | Color (`NAMES.TXT` `@COLORS`) |
|----------------------|------|-------------------------------|
| Fill | full rect | Caller tile sprite 0 (`OPENTILE.SS` on the title menu; typically `WOODTILE.SS` in-game). Solid index `4` if no sheet. |
| Outer | 1px all sides | Black (`0`) |
| Mid | 1px all sides | `border0` = **134** (WOODTILE mid brown) |
| Bevel | 1px | `border1` = **128** light on **top + right**; `border2` = **138** dark on **bottom + left** |

Content draws inside the rect inset by `POPUP_FRAME_INSET` (3). Call `popup_draw(...)`; on palettes that differ from WOODPANL (e.g. `OPENMENU.PIK`), use `popup_colors_from_ui` + `popup_colors_remap`. First consumer: title `@BEGINMENU`. Not the same as `WOODFRAM.SS` (the milestone-woodcut picture frame, `woodcut.c`) or map-menu pulldowns. Which game dialogs use this chrome (and port status): [popups.md](popups.md).

`.SS` sprite sheets (e.g. `TERRAIN.SS`, `CURSOR.SS`) use four MADSPACK sections: header, per-sprite metadata, palette, and linemode-compressed pixel data. Sprites are blitted with transparency at index `0xFD`. **Both** `CURSOR.SS` frames (the pointer and the Go-To hand) carry a stray index `0x09` (light blue) at the top-right and bottom-left corners of their 17x17 frame — a registration mark, not art; no other `.SS` in `COLONIZE/` has the pair. `ui_drag.c` drops those two cells on both build paths, so the SDL colour cursor shows them as transparent (port_bugs.md #373).

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
| 0 | 2 | map width (u16 LE) |
| 2 | 2 | map height (u16 LE) |
| 4 | 2 | version (u16 LE, 4; MAPEDIT `FUN_19f9_0170` refuses any other) |
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
The port decodes the index as `byte & 0x1f`.

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

**Loaded-map finalize (2026-09-15):** DOS runs the `FUN_684c_08c0` tail on a loaded `.MP` too (asm `684c:08e2` jumps to `LAB_684c_17f1` when the map came from file): two high-seas outline rings, arctic on rows 0 / h−1, bit-4 forest demote and hills/mountains lose their forest class. AMER2.MP ships 146 forested hill/mountain tiles that DOS therefore never draws with canopy; the port applies the same pass (`map_gen_finalize_border_and_forest`) on the Original Americas new-game path.

**Fidelity status:** static AMER2 art matches MAPEDIT for coasts, estuaries, land–land transitions, forest/hill/mountain/river connectivity, special resources, and rumours. **Plowed fields** use runtime PHYS0 **149**; **roads** use PHYS0 **80** isolated or multi-blit **81–88** directional stubs (`FUN_6ba1_0938`). Land MP uses DOS `terr_cost` table at NAMES scale (`map_move_cost_*`). No per-tile texture variation or coast animation exists in DOS (see Coastal ocean section); water shimmer is the CYCLE.DAT palette cycle.

Authority for static map art is decompiled **`MAPEDIT.EXE` /
`original_sources_decompiled/mapedit.c`** (no RTLink), not VICEROY’s runtime buffers.
Compile-time toggles `MAP_COAST_OVERLAYS_ENABLED` / `MAP_ESTUARY_OVERLAYS_ENABLED`
(default **1** in `src/core/map.h`) exist only to disable coast/estuary for debugging.

The port draws cleared terrain from `TERRAIN.SS` (bits 0–4), then composites `PHYS0.SS` in MAPEDIT order:

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

**Hill / mountain / river overlays (shared adjacency, 0-based indices; MAPEDIT `FUN_1a47_030e` / `036e` / `0418`):**

| Feature | PHYS0 | Neighbour match |
|---------|-------|-----------------|
| Mountain | `32 + mask` | `(n & 0xa0) == (self & 0xa0)` |
| Hill | `48 + mask` | same |
| Major river | `0 + mask` (mask 0 → 15) | any `n & 0x40` |
| Minor river | `16 + mask` (mask 0 → 15) | any `n & 0x40` |
| Forest | `64 + mask` | any non-scrub forest |

MAPEDIT immediates are **1-based** (`0x21`/`0x31`/`0x41` → indices 32/48/64). Debug atlas labels match these 0-based indices.

Hills/mountains come only from the terrain byte; layer 3 (continent / region id, e.g. AMER2 `(43,68)` = `0x0e`) never contributes art.

**Ocean estuaries.** Terrain index 25/26 with `terrain & 0xc0` marks river mouths; for each cardinal land neighbour with bit `0x40`, MAPEDIT blits ID **`0x8d+q`** (major) / **`0x91+q`** (minor) = 1-based 141–148 → indices **140–147**. Inland rivers unchanged. Fixtures: `amer2_coast_fixtures` / `amer2_river_estuary` (`tests/unit/test_map.c`).

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

**Coastal ocean.** Enabled by default. MAPEDIT (`FUN_1a47_01ae` masks): on an ocean tile with any land neighbour, build the 8-bit land mask (N→NW clockwise) and four 3-bit quadrant masks (cardinal land sets bit 2 on its quad and bit 0 on the next; diagonal sets bit 1). A matching mask gives one full-tile corner **150+id** (MAPEDIT `0x97+id`, land NW/NE/SW/SE); else four 8×8 fragments **108+4×mask+q** (MAPEDIT `0x6d+4×mask+q`) at NW/NE/SE/SW offsets 0/8. Order: land underlayer → coast → masked ocean into colour-0 holes (`FUN_1a47_0676`) → estuary (+ fish when present).

| Piece | PHYS0 / TERRAIN |
|-------|-----------------|
| Underlayer + zero-fill | land TERRAIN, then ocean into dest==0 |
| Fragments + corners | 108–139 (8×8), 150–153 (16×16) |
| Estuary cardinals | 140–147 (16×16) |

Older VICEROY quadrant / RAM-buffer coast heuristics are **superseded** by this MAPEDIT path.

**No per-tile texture variation, no coast animation frames (DOS verified 2026-10-28).** `FUN_6ba1_0d6c` walks the viewport and calls `FUN_6ba1_0938` per tile; its base blit `FUN_6ba1_05b8` -> `FUN_1101_0050` (asm 6ba1:05b8) copies the 16x16 block at `TERRAIN.SS` buffer (`DS:0x16c`, loaded by `FUN_74a4_0bbe`) + `FUN_1101_0026(class)*0x100`, `class` = `FUN_137f_0614(terrain byte)`; no x/y hash, RNG or per-tile RAM buffer enters the source offset. The `0xa598/0xa594/0xa59c` buffers are only the terrain/overlay/visibility map arrays, and `DS:0xa5a8` is a column-parity flag nothing reads for art. Coastline pieces (`FUN_6ba1_06e0`, `0x1ea4/0x1ea5` quadrant offsets) are static per tile. The only DOS "animation" of water is the CYCLE.DAT palette rotation (`game_water_cycle_tick`, already ported). Fog fringe (**104–107**), plow (**149**), and road **80–88** connectivity are drawn.

`src/data/viceroy_tables.{h,c}` holds the one remaining `VICEROY.EXE` extract, used only by Colonizopedia terrain previews ([viceroy_tables.md](viceroy_tables.md)). World-map **feature art** (forest/hill/mountain/coast) uses the MAPEDIT rules above.

### Map menu bar

On the main map, the top strip is the DOS menu bar from `MENU.TXT`: **GAME**, **VIEW**, **ORDERS**, **REPORTS**, **TRADE**, **COLONIZOPEDIA**, with fixed slots for **CHEAT** (`@CUP`) and (when built with `COLONIZE_DEBUG_MENU` and `debug.menu` is true) **DEBUG** between TRADE and COLONIZOPEDIA. The bar uses `WOODTILE.SS` fill (same as the right panel), `FONTTINY.FF`, `@COLORS` basic green with yellow (`~`) hotkeys, and a 1px black rule across the full width. Open pull-downs use the same wood fill (screen-aligned grain), a 1px black outline, green item text with yellow hotkeys, and sit **1px below** the bar rule so map pixels show in the gap. **Separators** are not in `MENU.TXT` (except PEDIA’s cosmetic `---`); DOS `FUN_74a4_0000` inserts empty-label rules into GAME / VIEW / ORDERS / REPORTS / CHEAT / PEDIA — the port mirrors those (plus one port-only rule between Founding Fathers and Miscellaneous in PEDIA, user-requested). ORDERS hide/gray follows `FUN_2b5a_0b34` (Move Pieces) / `0902` (View Pieces). Below the bar the screen splits into a **15×12-tile map viewport** (`x=0..239`, `MAP_PANEL_X` / `MAP_VIEW_*` in `src/core/map_panel.h`) and a **right info panel** (`x=240..319`, 80px) with a 1px black left edge. The world map is stored at 58×72 including a 1-tile rim; the viewport origin is inset by one tile (`FUN_6ba1_000c` / `map_panel_clamp_view_origin`) so the rim is never scrolled onto the main map (visible area 56×70). Click a title to open its pull-down; click an item to activate it (grayed items are stubs). Esc closes an open menu; with no menu open, Esc, Ctrl-Q, Ctrl-X, Alt-Q and Alt-X ask the `@DOSYES` quit confirm (default Yes) and Yes exits the program (`FUN_2b5a_303c` / `321c` / `3252` -> `2b5a:3104`); plain Q is not a quit key. Left-click on the map viewport: select an owned unit with moves, else select the tile (owned unit with no moves, empty land, etc.), or open an owned colony. Right-click always selects the tile (and clears unit selection). While a unit is selected, left-click only pans the viewport. Left-click on the panel minimap centers the view on that world tile. The selected tile shows a blinking white outline; over the map viewport the OS pointer uses `CURSOR.SS` #0. Forest terrain names combine the `NAMES.TXT @FORESTED` stem with `@OTHER_NAMES` row 0, producing labels such as **Tropical Forest**.

**CHEAT:** Hidden until unlocked. On the main map, press **Alt-W**, **Alt-I**, **Alt-N** in succession (spells WIN; any other key in between restarts the sequence, `FUN_2b5a_3094` / `FUN_2b5a_32a2`); with the menu showing, any of Alt-W/I/N hides it again. The unlock is the save-file bit `head.game_options.cheats_enabled` (DS:0x5383 bit 5): the title shows exactly while the bit is set, so a loaded save with the bit set shows CHEAT at once (`FUN_75c2_0940`) and the bit is saved. Layout still reserves the CHEAT title slot while hidden so DEBUG does not shift. With the bit set: Shift-F1..F10 are the CHEAT hotkeys, the Europe screen takes `$` / `]` / `[`, the colony screen takes T / S / `$` / `%` / `^` / `!` / Space (`FUN_2f2b_6372` tail, port_bugs.md #1173), and F10 (REPORTS Score) runs the retire chain instead of the Score report (`FUN_2f2b_6372`). Enabled items: **Reveal Map** (Shift-F4, DOS case 0x65) opens `DEBUG.TXT` `@SETVIEW` — English/French/Spanish/Dutch/Complete Map, plus No Special View only while a view is active (DOS passes 5 rows, else 6). This is a reversible fog viewpoint (does not rewrite `seen[]`) and the save head is its source of truth, as in DOS: a nation row sets `fixed_nation_map_view` (0x53a4) and `curr_nation_map_view` (0x5396) and saves/reloads as that view (the fixed view is a stale value and heals to 0xffff when it equals the human or the head's `human_player` disagrees with the control table); Complete Map sets `show_entire_map` (0x53a2) and clears the live `show_indian_moves` bit; Esc / No Special View reset both (`game_fog_nation` derives the map fog from them; port_bugs.md #1170). **Set Human Player** (Shift-F5, DOS case 0x66) makes the picked nation the human (control bytes, `human_player` / `nation_turn` / `curr_nation_map_view`); another nation reloads the live save in place so the Europe mirror (gold, tax, dock, trade) is rebound to that nation's own record (`game_cheat_set_human`); "None" hands every nation to the AI and asks @SETAUTO; Yes runs the game unattended until a click, No leaves a spectator ([demo.md](demo.md)). **Advance Revolution Status** (Shift-F7, DOS case 0x68) steps one stage per press: rebel sentiment to 75 (+ the War of the Spanish Succession when the Crown slot is empty), declare independence, announce the foreign intervention, then `independence_force` with the DEBUG.TXT `@FORCED` notice (`ai_king_cheat_advance_revolution`). **Kill Indians** (Shift-F6, DOS case 0x67 → `FUN_4d56_01e2`) lists the tribes that are not extinct (nothing opens when all are) and razes every village of the pick, highest tribe index first, exactly as a conquest does (`units_destroy_tribe_nation_w`: village-bound units go, `@EXTINCT` at the last village; owner nibbles, `indian[]` and diplomacy stay). **Create Unit** rows 9–12 (pre-WoI Indian types) belong to the tribe of the village nearest the cursor, no village = nothing created; every created unit is visible to all four nations (`vis_mask` 0xf). Shift-D in View mode (`FUN_2b5a_199e`): an own-owner tile (or any tile with CHEAT) disbands the stack head after the `@SUREDISBAND` confirm; with CHEAT an empty-of-units colony tile gets the same confirm and deletes the colony, and a village tile is razed with no confirm. All 11 CHEAT entries are live; player reference: [cheats.md](cheats.md).

**DEBUG** (CMake `COLONIZE_DEBUG_MENU`, shown when `debug.menu` is true): **Sprite Viewer** (same as `` ` ``), **Show Mouse Coords** (pointer pixel HUD; off by default, persisted as `debug.mouse_coords`), **Building Rects** (colony-screen sprite bounds; off by default, persisted as `debug.building_rects`), and **Debug Logs** (`diag_info` to `opencol.log`; off by default, persisted as `debug.logs`).

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
| Region id / cheat lines (`FUN_49dd_0424`, port_bugs.md #1177b) | `col1.head` | `special = show_entire_map != 0 \|\| fixed_nation_map_view != 0` on the raw u16s (0xffff "none" counts). Tile-only block (View / foreign unit): `Locat: (x,y) <n>` with `n` = raw `layer3 & 0xf` (FUN_137f_01ca, 1 = open ocean, water included) and **no cheat test**. Cheat bit and `special`: the same suffix on the unit block's Locat line. Cheat bit and Complete Map: `<unit index> ` ahead of the unit block's Nationality line and of every tile-stack row's orders line (`units_dos_index`). A foreign stack collapses to one row unless `top.nation == curr_nation_map_view` or Complete Map (the viewer's own stack, not the save's human). Cheat bit + `DS:0x894 & 8` ("Foreign AI planning modes", the port's `debug_flags_mask`): every AI-nation unit badge shows its AI plan byte (`+0x314b`, >= 0x80 = 'E', 0 = empty) instead of the order letter (`unit_chrome_plan_badge_index`; ink keeps following the real orders; the damaged repair digit still wins). The Debug Info Flags consumers in `game_loop.c` are deliberately ungated: DOS gates only that 0x894 & 8 arm on the cheat bit |
| Village trade intel (OpenCol-only) | `ICONS.SS` **#22–37** | Under the native camp line: `Buys:` row (goods named by @BRING / @BADCARGO / @CHIEFHOWDY) and `Sells:` row (@BUYWHICH), 13px rows, icons in one shared column; each row only once that dialog has been shown to the player. Side table `village_trade_intel.c` keyed by settlement tile and European nation — persisted in the save's port extension block (`VTIN` chunk, docs/savegame.md), wiped by `ai_contact_reset` (new game / Load, before `col1_bridge_apply` restores it) and on village removal. `render_map_panel … buys=a,b,c sells=a,b,c` seeds it |
| Nation box | Turn indicator at `(315,197)` | 5×3 fill in the acting nation's color (`FUN_1984_00aa`), painted at the head of **every** nation's end-of-turn — the human's own production pass included; dark during the human's own play. England **112** (NAMES lists 12), France 9, Spain 14, Netherlands 13 (`@COUNTRY`); native phases use `@TRIBES` colors |

Not used for this panel: `WOODPAN2.PIK` (score/fame chrome), `WOODFRAM.SS` (milestone-woodcut frame). Minimap fog: unseen tiles draw black (`map_panel_render_minimap`, `map_tile_seen_by`).

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
| F9 | Indian Adviser | `REPORT1.PIK` (`FUN_3f41_010a` loads plate 1; `REPORT9.PIK` is unused) |
| F10 | Colonization Score | `WOODPANL.PIK` (full-screen wood) |

Content uses `ColonizeCol1Save` when a campaign is loaded (crosses / founding fathers / tribes / trade ledger / rival strength), with runtime colony / unit / Europe pools as fallback. **F10** uses the manual score schedule (citizen quality, congress, gold/1000, rebel sentiment, village-burn penalty, independence multipliers when declare/achieve are tracked). **F3 Continental Congress** is two pages: page 1 (bells progress bar toward the next Founding Father, rebel/tory split, expeditionary force, FF name list) advances to page 2 (full-bleed Founding Father group portrait, no chrome) on Esc/Enter/click/OK, and page 2 itself closes on any click.

All nine F2-F10 report plates match their golden screenshots (P2.12 user-passed
2026-09-03). See **[report_screens.md](report_screens.md)** before touching any of
them; open gaps are in [reports.md](reports.md) "Current-state note".

### Colonizopedia

Open from **COLONIZOPEDIA** on the map menu bar, or press **P** (Cargo Types list). Each category opens an encyclopedia **list** on `WOODPANL.PIK`: white **ENCYCLOPEDIA OF COLONIZATION** header, green clickable entry titles in up to three columns (`FONTTINY.FF`; Founding Fathers uses two wider columns), and **(Exit)** top-right. Click an entry for the article; Esc / (Exit) / P from the list returns to the map.

**Articles** are DOS-fidelity recreations of the segment-`6cb2` builders (`FUN_6cb2_05ce` cargo / `07e6` unit / `0eac` terrain / `1820` skill / `1ba8` building / `1f28` father / `203c` misc — `pedia_article_render` in `src/core/pedia.c`): WOODPANL background, centered hilite header, centered hilite `(Name: Category)` title (category subtitles from `@PEDIA`), per-category preview + stats, then the `PEDIA.TXT` section body (`@width` wrap, prose flows left-aligned in basic green, a caret-led line gets an unwrapped line of its own — `^` left-aligned at the prose margin, `^^` centered (DOS `FUN_6f74_0c32` flags 2 / 1; PEDIA.TXT uses only `^`, so every heading is left) — `{...}` spans hilite). **Any key or mouse click dismisses an article** (DOS `FUN_281f_03c0` wait) — back to the list, or to the map when opened via F1. Category specifics: cargo pages show 1–3 producer rows (job figure + stacked goods icons + "Name (With Expert ...)", `FUN_6cb2_048c`); unit pages a chrome figure strip (Colonists = every profession, 17/row; Soldier-class types plain+veteran; Artillery plain+damaged) and a `Combat/Moves/Cargo Holds` stat line from `NAMES @UNIT`; terrain pages the double-framed 3×3 tile composite (forest/mountain/hill block pieces, river/road overlays, prime-resource center) plus per-job yield rows (`Plow|Road|Coast/River` and resource/Expert bonuses) and `Move Cost` / `Defense or Ambush Bonus` (`+25%×byte`); skill pages the expert figure, workplace upgrade chain from `BUILDING.SS`, and produced cargo; building pages the building art, worker + product row, and `Prerequisite:` line; father pages are text-only (DOS shows no portrait). DOS DS tables (job→chain `DS:0x2f4`, building→job `DS:0x2ca`, chain next/prereq, terrain→resource `DS:0x192`) were extracted from `dosbox-x-dumps/find_memory` and are hardcoded in `pedia.c`. Misc ("Game Concept") pages keep the port's short blurbs as bodies — DOS looks up `@MISC<n>` sections that don't exist in `PEDIA.TXT`, so its own misc pages are title-only.

The pull-down has a horizontal green rule between Terrain Types and Colonist Skills (DOS `FUN_74a4_0000` empty-label sep after Terrain; `MENU.TXT` `---` is ignored on load and the same sep is inserted in code).

| Menu item | PEDIA.TXT | Preview |
|-----------|-----------|---------|
| Cargo Types | `@CARGO0`–`15` | `ICONS.SS` #22–37 |
| Unit Types | `@UNIT0`–`22` (`@UNIT23` is a stub Colonists duplicate — dropped) | `ICONS.SS` (`NAMES.TXT` `@UNIT` icon) |
| Terrain Types | `@TERRAIN0`–`28` | 3×3 TERRAIN/PHYS0 composite |
| Colonist Skills | `@JOB0`–`27` (`@JOB18` Teacher/Student was cut pre-release — hidden from the list) | related cargo/unit icon |
| Colony Buildings | `@BUILDING0`–`41` (`30`/`31` Capitol / Capitol Expansion are cut — title-only stubs, unbuildable in DOS, hidden from the list) | `BUILDING.SS` (Stable → sprite 46) |
| Founding Fathers | `@FATHER0`–`24` | none (DOS text-only) |
| Miscellaneous | `@MISCELLANEOUS` titles | text blurbs (no PEDIA bodies in data) |

**F1** / **REPORTS → Terrain Information** jumps straight to the terrain article for the cursor tile (Esc exits to the map).

### Europe (home port) bring-up

Press **E** from the map to open the European Status screen (`EUROPE.PIK`). Esc or E returns to the map (closes open menus first). Visual layout reference: [`original_screenshots/europe/`](../original_screenshots/europe/). Harbor / Expected / Bound ships and dock immigrants draw orders/allegiance chrome (`unit_chrome.c`) behind their `ICONS.SS` sprites.

Transit (`europe_voyage_turns_roll`, DOS `FUN_48d3_0002`): **1 turn**, or **2**
when `RNG(1,100)>89 && ships>2 && !Magellan` — both directions.

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
| `$` / `]` / `[` | Cheat (only with `cheats_enabled` set): +1000 gold / tax +1 / tax −1 (`FUN_38fd_4f6e`) |

Market bid/ask from `NAMES.TXT` `@CARGO` (ask = bid + burden + 1). Sale proceeds `floor(bid × amount × (100 − tax) / 100)`. Volume-price T0 via `europe_apply_volume_price` (**Done thin**; EOT attrition + half `market_demand_pool`); boycotts **Partial** structural. No pressure / price-trend / bid chrome exists in DOS: `FUN_38fd_0b64` (raw 59293-59351) draws only the 16 cells' `bid/ask` text (`FUN_38fd_0040` ask / `FUN_38fd_0016` price), the red boycott tint (`FUN_38fd_05e8`) and the selected-cell highlight rect, which the port matches — [manual_gap.md](manual_gap.md).

### Colony screen bring-up

Press **C** on the map when the cursor is on a founded colony to open the colony screen. The layout matches DOS “THE COLONY DISPLAY”:

| Layer | Asset | Role |
|-------|-------|------|
| Wood chrome | `WOODPANL.PIK` (320×200) | Full-screen panel; supplies the colony-screen palette |
| Top bar | (rendered) | Colony name, campaign date, and treasury across the top strip |
| Settlement view | `PARCH.SS` + `BUILDING.SS` | Real DOS placement: 15 slots (`DS:0x266`) in 5 size classes, one per building category, shuffled per colony from `(y<<8)+x` + a pinned base seed — see [colony_screen.md](colony_screen.md). Workers + badges via Note 1 strips at building bottom-center (selectable); hit-test uses **sprite bounds**; outside units same on the fence slot; thin construction banner (project + hammers; click opens Change) |
| Area view | `TERRAIN.SS` + `PHYS0.SS` + `ICONS.SS` | Centered 3×3 at **24px** tiles (1.5×); settlement **#0–3**; Indian villages with their main-map chrome, fog-gated on `curr_nation_map_view`, drawn at 16px then scaled 1.5× with the terrain (`FUN_2f2b_0a74` tribe overlay + `FUN_12e9_008c` scaler); yields via resource-count strips (fisherman food uses fish **#57**) |
| Minimap panel | `WOODTILE.SS` (32×24 tile) | Wood-grain fill behind the 3×3 (equal L/R and T/B margins) |
| People view | `COLONY.PIK` left (x0–117) | SoL (flag **#123**) / Tory (crown **#124**, right-aligned); colonists + fence units on one row; food→crosses→bells resource strips (fish **#57** first, then grain food) |
| Transport view | mid (x121–202) | Ship **class** name; holds: color / grey partial / empty cover **#122** (all six covered with no ship); docked ships + passengers via `unit_chrome_blit_unit` |
| Multifunction | right (x207–306) + buttons (x307–318) | Production / Units / Construction (house **#67** / rifle **#68** / hammer **#69**); Production packs all cargo+shortfalls+hammers; **Units** rosters land units at the colony — colonist-class + Artillery, not an armed-only subset, but never ships/wagons (those stay on the Transport strip); 2nd click opens the same docked-unit orders popup as the Transport strip ([`colony_screen_multi_units_layout`](../src/core/colony_screen.c); DOS `FUN_2f2b_1e46`/`59a0` share `FUN_2f2b_5746`); Construction shows accumulated hammers |
| Warehouse | cargo strip `y=180/192` | Unchanged slots (`ICONS.SS` **#22–37**); Exit `x≥305` |

**Resource-count strip** (`colony_screen_draw_resource_count` / `_pair` / `draw_icon_strip`): one icon per unit, evenly spaced in a rect; when start-to-start spacing is ≤ 1px (nearly total overlap), overlay the amount on the **left** with a black outline and a caller-chosen foreground (white / green / red / …). An `always_show_number` flag exists but is unused by current call sites. Used for area yields, people meters, Production tab, Construction accumulated hammers (two rows of progress toward the project, not total cost), and settlement building/fence colonists. Fisherman yields still count as food cargo; colony UI only swaps the icon to `ICONS.SS` **#57** (fish before grain when both appear in one strip). Colonist/unit strips pass a `selected_index` so individual icons are selectable; resource strips pass `-1`.

`CLOS-BKG.PIK` is the independence closing-sequence backdrop (CLOSING.EXE / `closing.c`) — not used on the colony screen.

Single-pixel black separators split the sections: top bar vs middle band, buildings vs minimap, and middle band vs bottom panel.

A new colony gets the classic free starters: Town Hall, Carpenter's Shop, Blacksmith's House, Weaver's / Tobacconist's / Distiller's / Fur Trader's houses. Warehouse, Stockade, and Docks are **not** free. Warehouse cargo starts empty (DOS `FUN_364b_1ba8` clears stock); founding unit gear still dumps tools/muskets/horses. Founding defaults `building_in_production` to **Stockade**. Until Stockade is built, the fortification strip shows the post-and-rail fence (`BUILDING.SS` **#16**) bottom-right. Coastal colonies without Docks show empty coast (`BUILDING.SS` **#45**) above the fence. Empty building slots use tree clumps (`BUILDING.SS` 42–47). Founding with **B** disbands the map unit into a Town Hall colonist (skill/`profession` preserved) and dumps carried tools/muskets/horses into the warehouse.

**Drag or select-then-click** assignment: drag a colonist (or fence unit) onto a building, area/minimap tile, or the fence; short click still selects, and a second click assigns. Outside units are admitted into the colony on assign; carried tools/muskets/horses go to the warehouse. Drop/click the fence with a colony colonist selected to open a **Leave as** popup (Colonist / Pioneer / Soldier / Scout / Dragoon / Missionary if Church or Cathedral, gated by stock). Working colonists use skill sprites (Hardy Pioneer **#58**, Veteran Soldier **#59**); outside/map icons follow equipment (pioneer/soldier/scout/dragoon; expert sprites only when `profession` matches that role — e.g. hardy pioneer with muskets uses non-veteran soldier **#74**). Skill sticks across admit/eject and gear changes. The people band shows colony colonists and fence units (no Artillery/Treasure) on the same row (outside group to the right, with a gap; [colony_screen.md](colony_screen.md#people-band)). Production preview (`colony_preview.c`) drives area/settlement badges, people meters, and the Production tab without mutating stock. Town Hall / Church / Cathedral settlement badges include free bells/crosses even with no worker assigned. The Production multipurpose pane lists every produced cargo type (grey shortfall rows) plus hammers, packing slots into a dynamic grid as the set of types changes; crosses/bells stay on the people meters. With no docked/selected transport, all six hold slots show empty-hold covers. SoL uses Col1 `rebel_dividend`/`rebel_divisor` when the colony is bridged from a save; otherwise 0% SoL / 100% Tory.

| Key / click | Action |
|-------------|--------|
| Click / drag colonist → building / area / fence | Select; assign workplace, open field-jobs popup, or Leave-as |
| **N** / click Production pane | Toggle production numbers on dense strips |
| **1** / **2** / **3** / **M** | Multifunction Production / Units / Construction / cycle |
| Construction **BUY** / **CHANGE** | Buy remaining project (multifunction BUY); Change popup lists projects only (no Buy row; **C** also opens Change) |
| **B** / Buy | Finish current project: gold = remaining hammers; spend `tools_cost` from warehouse |
| Drag / click ship cargo / hold | Select transport; drag warehouse↔hold to load/unload |
| Fence **Leave as** | Eject/equip popup (Colonist / Pioneer / Soldier / Scout / Dragoon / **Missionary** if Church/Cathedral); last colonist confirms **abandon** (cargo lost); Stockade/Fort/Fortress cannot drop below 3 colonists |
| **L** / **U** | `L`: load the highest-key warehouse cargo, key = min(stock, 100) * (sell price << 4), Muskets/Tools 1, Horses 0 under 102 stock (FUN_479b_11a4; red bars when no hold is free / nothing to load); `U`: unload the lowest-key hold (FUN_479b_0f60 mode 0) |
| **=** / **+** | Full / partial load of selected warehouse cargo |
| Esc | Close message / jobs / eject / construction, then return to map |
| Enter | Confirm popup; with a colonist selected open field jobs; else leave |
| Space | CHEAT bit only: one extra production tick of the viewed colony (`FUN_364b_0688`); ignored otherwise |
| **T** / **S** / **$** / **%** / **^** / **!** | CHEAT bit only: add a colonist / selected colonist's profession := his job / +1000 gold / selected good +100 / -100 / every building (docs/colony.md "Colony-screen cheat keys") |

### Units (map bring-up)

Unit stats come from `NAMES.TXT` `@UNIT` (icon index, movement, land vs sea via hull field). Map markers use `ICONS.SS` sprites; the `@UNIT` icon field is **1-based** (DOS style), converted to 0-based blit indices on load (e.g. Pioneers `102` → sprite **101**, Caravel `6` → **5**). Cargo strip icons `#22–37` are already 0-based (greys `#38–53`). European colonies blit settlement art **#0–3** (none / stockade / fort / fortress; 21×16, centered on the tile) with a name label under the tile; Indian villages blit **#10–13** by `@TRIBES` tech (tipis / adobe / pyramid / city). Draw order is colonies → villages → units so stacked units stay visible.

`@UNIT` row names are plural catalog labels (`Colonists`, `Pioneers`, `Soldiers`, `Dragoons`, `Scouts`, …), not display strings — `units_display_name()` derives the singular UI name from base type + equipment + profession (armed→Soldier/Veteran Soldier, mounted→Scout/Seasoned Scout, tools→Pioneer/Hardy Pioneer, else `Pioneers`→Pioneer / `Soldiers`→Soldier / **`Colonists`→Free Colonist**). The bare-`Colonists` branch is load-bearing: every `strstr(units_display_name(...), "Free Colonist")` gate in the port (`ai_contact` teach-skill, `ai_euro` LABOR joins / founding-site picks, …) depends on it to recognize an ordinary colonist against real game data — there is no separate `Free Colonist` row in `@UNIT`.

**Orders / allegiance chrome** (`unit_chrome.c`, DOS `FUN_112b_01ba`): each unit graphic is a black silhouette shifted 2px left, then a nation-colored orders rectangle (`@ORDERS` letter), then the color sprite. Placement by Col1 type: foot units bottom-right; mounted / Caravel / Merchantman top-left; Galleon–Man-O-War top-right; Treasure / Artillery / Wagon top-center. A second under-rect marks stacks. The box fill is the raw `@COUNTRY` index DS:0x848 (England 12, France 9, Spain 14, Dutch 13; tribes 4..11 = the `@TRIBES` colour column 97 149 54 87 67 111 118 71, a live DOS read of DS:0x848 — the EXE's static image differs and is overwritten at start-up), the letter index - 8, drawn at pen x = box x + 2 (112b:074a..0758, cell origin, not ink-centred); under the VICEROY.PAL DAC that is (255,0,0) / (85,85,255) / (255,255,85) / (255,113,0). Picture and sheet palettes (TERRAIN.SS, WOODPANL.PIK, EUROPE.PIK, REPORT*.PIK) carry pink / EGA magenta at 5/12/13 instead, so screens presented through a non-VICEROY palette nearest-match VICEROY's RGB via `unit_chrome_*_for_palette` (port_bugs.md #364). Wired on the main map, sidebar, Europe docks/ships, colony Units + transport panes, and Colonizopedia unit previews.

Terrain move costs (plains 1, forest/hills 2, mountains 3; road/river halves) follow DOS `FUN_465b`: a unit may always enter when it still has its **full** movement allotment (remaining MP then exhausts). With **partial** MP into an over-budget tile, the game rolls `range(1, cost)` and succeeds if `roll ≤ remaining`; the full cost is charged either way (failed rolls leave the unit in place with 0 MP). Go-To pathfinding only commits guaranteed steps (no gambling) — so a Go-To / trade-route unit left with a third or two of MP in rough terrain has no legal step; it then **parks for the turn** (remaining MP dropped) instead of being handed back to the activation cycle, which used to hold the cursor on a piece that could never move (port_bugs.md #363).

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

Ship→**native village**: never landfall — unmet `@DONTKNOWSHIPS`, met/angry `@MADATSHIPS` (DOS `4528` ship abort). Ship→**non-colony bare land** with cargo: **landfall CHOICE** (`@LANDFALL` Stay With Ships / Make Landfall); any passenger with spent < max MP is eligible, **sentry** cargo included (`FUN_4720_015c`). Make Landfall unloads **one** passenger, who spends his whole allotment, and wakes the rest of the cargo (orders 0, port_bugs.md #1326); the ship is neither moved nor charged (port_bugs.md #716). Ship→**own colony** land: dock and **disembark all** (clear sentry). Land→ocean with an own ship that has room: **board**. Sentry land units on a ship's tile **auto-board** when that ship leaves (colony or ocean stack). Multi-unit tile click opens a wood **stack popup** — first click wakes sentry cargo, second selects; awake cargo can walk onto land to disembark. **O**/**U** remain. Enter rules: [move_enter.md](move_enter.md).

Selected units **blink** (sprite on/off), except while executing **Go-To** (always drawn so pathing stays visible). The tile cursor is shown only when no unit is selected. Units with `moves == 0` cannot be selected (tile under them is selected instead). Awake passengers (sentry cleared) with moves can be selected from the stack popup. When the active unit spends its last move, the next human unit with moves is selected; if none remain, tile-select mode resumes.

**Go-To:** drag from a blinking unit to a destination tile (CURSOR.SS #1 appears after ≥1 logical pixel of drag; pathfinding uses DOS-style destination cost flood nearby, BFS farther), or ORDERS **Go to Place** / **Go to Port** (the `@TRAVELPLACE` / `@SAILPORT` destination list, docs/unit_orders.md). The unit walks at **10 steps/sec** until out of moves and resumes after end of turn; **Fast Piece Slide** shortens the interval to 80ms (12.5 steps/sec). During interactive AI turns, **Show Indian Moves** / **Show Foreign Moves** present nearby AI tile steps using the same pacing. Sub-pixel slide animation remains final polish. Order byte is `@ORDERS` index 3.

Calendar: one turn per year until **1600**, then Spring and Autumn each year. Colony top bar / reports use `game_year` + `game_autumn`. When the Col1 **Autosave** option is set, end-turn writes slot **9** (and slot **8** on decade Spring years).

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
| text (glyph shade 1) | `basic` | 68 | 85,150,52 (green) |
| glyph shade 2 | `enhance` | 128 | 121,73,52 (brown-orange) |
| glyph shade 3 | `shadow` | 47 | 12,12,12 (near-black) |
| selection fill | `select` | 138 | 60,32,24 (dark brown) |

The text and its `_` caret are drawn once through the dialog writer's shade map,
like every caption around it (DOS `FUN_6f74_1e14` -> `FUN_6f74_0538`; no drop-shadow
pass, port_bugs.md #1191, [popups_dialog_engine.md](popups_dialog_engine.md) "Dialog text shading"). Both fields are
boxed by the same 1px green rectangle (`basic` ink). The wizard's box comes from
`text_edit_frame_rect` (a `TEXT_EDIT_FRAME_PAD` 3px gap around the font's real ink
rows). The in-game dialogs (`@COLONY` / `@RENAMECOLONY` /
`@LANDHO` / `@TRADENAME` / `@HOWMUCH*`) are laid out by `popup_field_frame`
(popup.c; geometry in popup.h, port_bugs.md #1181): the GAME.TXT line after the
prompt is the field's caption, and the box is `width("W") * max length + 6` wide
and `glyph_h + 5` tall, placed after the caption, with the text 3px inside it
and a pre-selected default shown on a select-ink bar (`text_edit_render` with
`line_h = glyph_h + 1`). Dialog width follows `@width` (`max(80, @width,
caption + field + 10) + 6`), the body wraps inside it, and the whole frame is
centred at (160 - W/2, 100 - H/2). `@LANDHO` (MSS0 admiral) and `@COLONY` (MSS5 nun)
latch an MSS figure first (`2b5a:0028`, `479b:0823`): `popup_field_frame` takes the
sheet, re-places the frame by `FUN_6f74_14c6` (`6f74:18ae-1a0c`) and blits the figure
over it (port_bugs.md #1183); the other prompts are bare.

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

VICEROY (not MAPEDIT) builds random maps in `FUN_684c_08c0` (dispatched via
`FUN_2a1f_083e`; land blobs `FUN_684c_02a8`; continent labeling `FUN_67bf_0000`).
The port mirrors that pipeline in `src/core/map_gen.c` into the same three-layer
layout as `.MP` files: terrain filled, `FUN_67bf_0000` continent ids in the layer-3 low
nibble, layer 2 cleared then given the pacific (`0x20`) / offshore-suppress (`0x04`) bits. Size is fixed **58×72** (`0x3a`×`0x48`).
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

**Fixed seed (`--seed N`):** the port has one chokepoint for the DOS timer word (`game_pick_rng_seed` in `src/core/game_loop.c`; `FUN_281f_04ca` = srand(BIOS tick), the PUSH of DS:0x83a6 before it is a dead 0 argument, and VR_SEED patches the tick read `FUN_1c0c_0012` to 100). `--seed N` (including `0`) overrides every timer-word read: NEW WORLD/CUSTOMIZE mapgen + `FUN_6a09` tribe reseed, Brave pulses, per-nation `ai_nation_reseed`, and the load-save LCG continuation. `--seed 100` reproduces the VR_SEED golden series, so DOS (VR_SEED patch) and the port can be run side by side to find divergence. Sound `pick_rng` is a fixed private constant (no game-state effect). Without `--seed` (and with `settings.json` `"seed": null`) the fallback is `elapsed_ms`, and every 04ca reseed reads the clock at call time (`dos_rng_04ca_word`).

| Extension | Typical use |
|-----------|-------------|
| `.PIK` | Packed pictures / backgrounds |
| `.SS` | Sprite sheets / animation frames |
| `.PAL` | VGA palette (`VICEROY.PAL`, 1024-byte file; DOS `FUN_7a7c_000e` reads the first 0x300 = 256 6-bit RGB triples: the master DAC) |

**Screen DAC.** `FUN_75c2_2d46` reads VICEROY.PAL once into A000:FC00 (`FUN_2a1f_0e28` ->
`FUN_7a7c_000e`); full-screen pictures with their own palette (WOODPAN2 `FUN_41f2_0b70`, CCBKGD
`FUN_4345_024a`) program all 256 entries and then restore from that buffer (`FUN_281f_03f4(.., 0xfc00)` ->
`FUN_1ade_0004`), reports program 152..251 only (`FUN_3f41_0000`), and the water cycle rotates
120..127 (`FUN_1a0a_007a`). WOODPANL, EUROPE and COLONY load through `FUN_291f_087a` ->
`FUN_7939_000c`, which reads pixels only, and no `.SS` palette reaches the DAC. So the map,
colony and Europe screens all draw through VICEROY.PAL (DOSBox map dump
`dosbox_save_state_2/Vga`: VICEROY.PAL at 0..151 with 120..127 rotated;
`original_screenshots/europe`: VICEROY's blue ramp at 54..59, not EUROPE.PIK's). The port sets
`game->map_palette`, the colony `frame.palette` and the Europe `background.palette` to
VICEROY.PAL (TERRAIN.SS's palette only if VICEROY.PAL fails to load).
| `.COL` | MicroProse sound drivers (`A/G/P/RSOUND.COL`) + `CONFIG.COL` |
| `.MOV` | Short motion / script tables (e.g. `AMERICA.MOV`; not the LEVN voyage) |
| `.MP` | Map data |
| `.DAT` | Tables / path data |
| `.BIN` | Large binary blobs (e.g. `COLDIG.BIN` digital SFX — decoded and played, see Music / sound) |


---

## Sound and Music

Comprehensive GSOUND driver reverse-engineering, DOS BGM scheduler, and sound ID ranges: **See [assets_sound.md](assets_sound.md)**
