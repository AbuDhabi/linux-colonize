# Colonization Asset Notes

Runtime data lives under `COLONIZE/` (override with `--data-dir`).

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
| `MENU.TXT` | In-game map menu bar (`@GAME` `@VIEW` `@ORDERS` `@REPORTS` `@TRADE` `@PEDIA`, optional `@CUP` cheat) |
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

The Linux port decompresses MADSPACK/FAB and blits `.PIK` images. The main menu uses `OPENMENU.PIK` with its embedded palette. `CCBKGD.PIK` is the Continental Congress / Founding Fathers background.

`.SS` sprite sheets (e.g. `TERRAIN.SS`, `CURSOR.SS`) use four MADSPACK sections: header, per-sprite metadata, palette, and linemode-compressed pixel data. Sprites are blitted with transparency at index `0xFD`.

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
| 6+W×H | W×H | layer 2 (unused in shipped maps) |
| 6+2×W×H | W×H | layer 3 (fog / visibility in-game) |

Each terrain byte (FreeCol `ColonizationMapLoader`):

- bits 0–4: terrain index 0–26 (see table below; bit 3 and bit 4 both contribute to the index)
- bits 5–7: overlay (0=none, 1=hill, 2=minor river, 5=mountain, 6=major river, …)

Forest indices 8–15 use bit 3; 16–23 use bit 4 (same eight forest types on cleared land 0–7).
The Linux port decodes terrain as `byte & 0x1f`.

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

`PHYS0.SS` contents (for map work):

| Sprites | Content |
|---------|---------|
| 0, 16 | Blank |
| 1–15 | Major rivers |
| 17–31 | Minor rivers |
| 32–47 | Mountains |
| 48–63 | Hills |
| 64–79 | Mixed forests |
| 80–88 | Roads |
| 89 | Depleted silver |
| 90 | Oasis |
| 91–95 | Wheat, cotton, tobacco, sugar, ore (flat) |
| 96–99 | Fish, beaver, deer, timber |
| 100 | Empty |
| 101–103 | Silver, ore (hill), rumours |
| 104–111 | Fog-of-war edges (approx.) |
| 112–127 | Coastline 8×8 fragments (edge variants; not yet placed) |
| 128–131 | Coastline 8×8 diagonal (checkerboard 2×2) |
| 132–139 | Coastline 8×8 fragments (not yet placed) |
| 140–147 | Coastline 16×16 animation frames (approx.) |
| 148 | Ocean overlay |
| 149 | Plowed |
| 150–153 | Coastal ocean corners (NW/NE/SW/SE land → 150/151/152/153) |

### Map overlay compositing

The Linux port draws cleared terrain from `TERRAIN.SS` (using FreeCol-style decoding of bits 0–4), then composites `.MP` overlays from `PHYS0.SS`.

**Terrain byte decode (layer 1):** `terrain_index = byte & 0x1f` (FreeCol). Indices 0–7 are cleared land; 8–23 are forests (type = `index & 7`); 24–26 are arctic/ocean/high seas.

**Forest TERRAIN/PHYS0 rules:**

| Forest type (`index & 7`) | Example index | TERRAIN sprite | PHYS0 overlay |
|---------------------------|---------------|----------------|---------------|
| 0 boreal | 8, 16 | 0 (tundra) | 70 |
| 1 scrub | 9, 17 | 8 | — |
| 2 mixed | 10, 18 | cleared type (plains) | 64 |
| 3 broadleaf | 11, 19 | cleared type (prairie) | 65 |
| 4 conifer | 12, 20 | cleared type (grassland) | 66 |
| 5 tropical | 13, 21 | cleared type (savannah) | 69 |
| 6 wetland | 14, 22 | cleared type (marsh) | 67 |
| 7 rain | 15, 23 | cleared type (swamp) | 68 |

Mixed/broadleaf/conifer/tropical/wetland/rain overlays use the `PHYS0.SS` mixed-forest band (sprites 64–79); one canonical sprite per type above. Sprite 99 is timber (bonus resource), not forest canopy.

Row `y=0` land tiles display as cleared tundra (sprite 0) with PHYS0 forest sprite 65.

**Overlay rules (bits 5–7):**

| Overlay | PHYS0 range | Notes |
|---------|-------------|-------|
| 0, 4 | (none) | — |
| 1 hill | 48–63 | Unless bit 4 is set → mountain (below) |
| 2 minor river | 17–31 | Cardinal mask (any river neighbours) → sprite |
| 3 hill + minor river | mountain or hill, then river | Bit 4 selects mountain vs hill |
| 5 mountain | 32–47 | Isolated tile uses sprite **36** |
| 6 major river | 1–15 | Major-neighbour mask, or minor-band at junctions |
| 7 mountain + major river | mountain then river | — |

**Ocean estuaries — parked (not drawn).** Terrain index 25 with river overlay (bits 5–7) marks river mouths in `.MP` data; the port draws **TERRAIN only** (`MAP_ESTUARY_OVERLAYS_ENABLED 0`). Parked `phys0_estuary_sprite()` in `src/core/map.c` — see [decomp_inventory.md](decomp_inventory.md) (**Parked: coastlines and estuaries**).

When overlay is 1/3 **and** bit 4 is set in the terrain byte, the tile uses mountain art (e.g. AMER2 `(1,1)` → PHYS0 36 on tundra).

Forests on other rows now draw PHYS0 canopy overlays; roads, resources, and fog overlays are not drawn from static `.MP` data yet.

**Coastal ocean — parked (cosmetic, not drawn).** `MAP_COAST_OVERLAYS_ENABLED` defaults to `0` in `src/core/map.h`. Research and resume checklist: [decomp_inventory.md](decomp_inventory.md) (**Parked: coastlines and estuaries**).

Prior documented models (also wrong / superseded):

| Model | PHYS0 | Status |
|-------|-------|--------|
| 2×2 full/diagonal corners | 150–153 (16×16), 128–131 (8×8) | Removed from code; do not revive without DOS proof |
| 4-quadrant neighbour mask | 108–139 (8×8) | **Parked**; `MAP_COAST_OVERLAYS_ENABLED` |
| Estuary lookup (DOS RAM capture) | 108–139, 149 | **Parked**; `MAP_ESTUARY_OVERLAYS_ENABLED` |

Land-side shore (140–153), animation frames (140–147), and per-tile texture variation from DOS RAM buffers are not drawn.

Tile compositing tables extracted from `VICEROY.EXE` live in `src/data/viceroy_tables.{h,c}`; see [viceroy_tables.md](viceroy_tables.md).

### Map menu bar

On the main map, the top strip is the DOS menu bar from `MENU.TXT`: **GAME**, **VIEW**, **ORDERS**, **REPORTS**, **TRADE**, **COLONIZOPEDIA**. The map viewport starts below that bar (`MAP_MENU_BAR_H`, 9px) so the top tile row is not covered. Click a title to open its pull-down; click an item to activate it (grayed items are stubs). Esc closes an open menu; Esc with no menu open returns to the title screen. Left-click on the map (outside menus) moves the cursor. The CHEAT (`@CUP`) menu is parsed but hidden until cheat unlock exists.

Working items today: Save/Load, Retire, Exit, European Status, Find Colony, Center View, Activate unit, Wait for next unit, Build/Join Colony, Load/Unload Cargo (board/unload), Return to Europe, No Orders (end turn), full **COLONIZOPEDIA** menu (cargo / units / terrain / skills / buildings / fathers / misc; divider after terrain), **F1** terrain info at cursor, and **REPORTS** F2–F10. Trade menu entries still stub.

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

The pull-down has a horizontal rule between Terrain Types and Colonist Skills.

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

Press **E** from the map to open the European Status screen (`EUROPE.PIK`). Esc or E returns to the map.

| Key | Action |
|-----|--------|
| R | Recruit cheapest `@CLASS` immigrant onto the docks |
| T | Train (stub) |
| S | Sail oldest harbor ship to the New World (onto a high-seas tile) |
| `]` | Cheat: +1000 gold |
| `[` | Cheat: −1% tax |

Market bid/ask prices come from `NAMES.TXT` `@CARGO` (ask = bid + burden + 1). Commodity cargo holds and drag-trade are not implemented yet. Unit passengers ride in the ship hold (board with **O**, unload with **U**) and persist through Europe harbor sail (**H** / **S**). Buying ships in Europe is not implemented yet.

### Colony screen bring-up

Press **C** on the map when the cursor is on a founded colony to open the colony screen. The layout matches DOS:

| Layer | Asset | Role |
|-------|-------|------|
| Wood chrome | `WOODPANL.PIK` (320×200) | Full-screen panel; supplies the colony-screen palette |
| Top bar | (rendered) | Colony name, campaign date, and treasury across the top strip |
| Building ground | `PARCH.SS` (32×24 tile) | Beige scrollwork tiled across the full upper-left buildings section |
| Buildings | `BUILDING.SS` | Starter buildings on parchment (indices match `NAMES.TXT @BUILDING`); **#16** fence (bottom-right); **#45** empty coast above fence; **42–47** tree clumps |
| Minimap panel | `WOODTILE.SS` (32×24 tile) | Wood-grain fill for the square top-right section (equal L/R and T/B margins) |
| Surroundings | `TERRAIN.SS` + `PHYS0.SS` | 3×3 catchment tiles centered in the WOODTILE section |
| Bottom panel | `COLONY.PIK` (320×72) | Outside colony / dock band; warehouse cargo strip along the bottom |
| Cargo icons | `ICONS.SS` **#22–37** | One icon per `@CARGO` type, centered in each 18px slot with the amount below |

`CLOS-BKG.PIK` is the independence closing-sequence backdrop — not used here. Esc, C, or Enter returns to the map.

Single-pixel black separators split the sections: top bar vs middle band, buildings vs minimap, and middle band vs bottom panel.

A new colony gets the classic free starters: Town Hall, Carpenter's Shop, Blacksmith's House, Weaver's / Tobacconist's / Distiller's / Fur Trader's houses. Warehouse, Stockade, and Docks are **not** free. Until Stockade is built, the fortification strip shows the post-and-rail fence (`BUILDING.SS` **#16**, one sprite) in the **bottom-right** of the buildings section. Coastal colonies without Docks show the empty coast placeholder (`BUILDING.SS` **#45**) **above** that fence; Docks / Drydock / Shipyard replace it when built. Empty building slots use tree clumps (`BUILDING.SS` 42–47). Founding with **B** disbands the map unit into a Town Hall colonist and dumps carried tools/muskets/horses into the warehouse (`stock[]` in `@CARGO` order; Pioneers → 100 tools, starter food 200). The bottom of `COLONY.PIK` holds the 16 cargo slots (18px wide, pitch 19, measured from the asset) with `ICONS.SS` icons and amounts underneath. Production, build queue, field work, and cargo drag UI are not implemented yet. Building collage positions are approximate.

| Key | Action |
|-----|--------|
| Esc / C / Enter | Return to map |

### Units (map bring-up)

Unit stats come from `NAMES.TXT` `@UNIT` (icon index, movement, land vs sea via hull field). Map markers use `ICONS.SS` sprites indexed by the `@UNIT` icon field (e.g. Pioneers → 102).

| Key | Action |
|-----|--------|
| Enter | Select unit under cursor (if different from current), or move selected unit to empty cursor tile |
| C | Enter colony screen when cursor is on a colony tile |
| D | Deploy oldest Europe-dock immigrant as a Colonist on cursor tile (land only) |
| O | Board: selected land unit onto ship under cursor, or selected ship loading land unit under cursor (must be adjacent; hold uses `@UNIT` cargo) |
| U | Unload oldest passenger from selected ship onto adjacent enterable land under cursor |
| H | Sail selected ship to Europe (must be on a high-seas tile, terrain index 26); passengers stay aboard in harbor |
| B | Found a colony on the cursor tile: land unit is disbanded into a colony colonist; tools/muskets/horses go to the warehouse stub (ships cannot found) |
| Space | End turn (`src/core/turn.c`): advance calendar (`@TIMECHANGE`), run colony production + nation ticks, AI/Indian/King stubs, refresh human movement |
| ORDERS → Wait | Select next human unit with remaining moves (“Continue turn.”); if none and End of Turn option is set, show “End of Turn” |

On the **colony screen**, Space is the DOS cheat **free production turn** (production only; does not advance year).

Calendar: one turn per year until **1600**, then Spring and Autumn each year. Colony top bar / reports use `game_year` + `game_autumn`. When the Col1 **Autosave** option is set, end-turn writes slot **9** (and slot **8** on decade Spring years).

A **5×3** nation-color box appears in the bottom-right `(315,197)` only while end-of-turn
nation phases run (`FUN_1984_00aa`): England 12, France 9, Spain 14, Netherlands 13
(`NAMES.TXT` `@COUNTRY`). Native phases use `@TRIBES` colors. It is hidden during the
human turn.

A **Pioneer** spawns at the AMER2 scenario start tile `(39,10)` when starting a new game, with a **Caravel** on the nearest ocean tile. Europe keeps dock immigrants until deployed with **D**. Press **B** with a land unit on a land tile to found a colony (unit becomes a Town Hall colonist; name comes from `COLONY.TXT @ENGLISH`). Ships move only on water; land units only on land. Boarded units are hidden from the map until unloaded.

| Extension | Typical use |
|-----------|-------------|
| `.PIK` | Packed pictures / backgrounds |
| `.SS` | Sprite sheets / animation frames |
| `.PAL` | VGA palette (`VICEROY.PAL`, 1024 bytes = 256×RGBA, 6-bit VGA RGB) |
| `.COL` | MicroProse sound drivers (`A/G/P/RSOUND.COL`) + `CONFIG.COL` |
| `.MOV` | Short motion / script tables |
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

Song names for the Pick Music UI are only in `GAME.TXT` `@PICKMUSIC` (plus Independence /
Military / Indian sublists). Options are `@SOUNDOPTIONS` and Col1 `tut2` bits.

**Playback — parked.** Heuristic F4→MIDI decode does not match the original soundtrack
(`COLONIZE_SOUND_PLAYBACK_ENABLED` is `0` in `src/core/sound.h`). The loader, ID map, and
API remain for a later fidelity pass. When re-enabled, FluidSynth uses `COLONIZE_SOUNDFONT`
or `/usr/share/sounds/sf2/*.sf2`; `--nosound` skips the SDL device. `smoke_sound` still
checks that `GSOUND.COL` songs resolve.

## Discovery Order

1. Explicit `--data-dir`
2. `<executable-dir>/COLONIZE`
3. `./COLONIZE` (working directory)
