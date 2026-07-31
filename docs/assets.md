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

**Fidelity status:** static AMER2 art matches MAPEDIT for coasts, estuaries, land–land transitions, forest/hill/mountain/river connectivity, special resources, and rumours. Remaining gaps are fog-of-war, roads, coast animation, and per-tile texture variation (see [decomp_inventory.md](decomp_inventory.md)).

Authority for static map art is decompiled **`MAPEDIT.EXE` / `mapedit.c`** (no RTLink), not VICEROY’s runtime buffers. Compile-time toggles `MAP_COAST_OVERLAYS_ENABLED` / `MAP_ESTUARY_OVERLAYS_ENABLED` (default **1** in `src/core/map.h`) exist only to disable coast/estuary for debugging.

The Linux port draws cleared terrain from `TERRAIN.SS` (bits 0–4), then composites `PHYS0.SS` in MAPEDIT order:

1. **Base** — land TERRAIN, or coastal **underlayer** (last cardinal land neighbour’s TERRAIN)
2. **Land transitions** (`FUN_1a47_06da`, land tiles only) — PHYS0 **104+q** colour-0 edge, then neighbour TERRAIN into holes; ocean neighbours resolve via land cardinals W→S→E→N
3. **Forest canopy** — PHYS0 **64+mask** (non-scrub)
4. **Overlays** — coast fragments/corners; hills/mountains/rivers; resources; rumours; estuaries  
   On coast tiles: coast PHYS0 → masked ocean into palette-0 holes → resource/estuary layers

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

Roads and fog overlays are not drawn yet.

**Coastal ocean.** Enabled by default. MAPEDIT: land underlayer → fragments **108+4×mask+q** / corners **150–153** → masked ocean into colour-0 holes → estuary (+ fish when present). Details: [decomp_inventory.md](decomp_inventory.md).

| Piece | PHYS0 / TERRAIN |
|-------|-----------------|
| Underlayer + zero-fill | land TERRAIN, then ocean into dest==0 |
| Fragments + corners | 108–139 (8×8), 150–153 (16×16) |
| Estuary cardinals | 140–147 (16×16) |

Older VICEROY quadrant / RAM-buffer coast heuristics are **superseded** by this MAPEDIT path (`docs/viceroy_tables.md`).

Not drawn yet: fog of war; roads; per-tile texture variation from DOS RAM buffers; coast animation frames.

Tile compositing tables extracted from `VICEROY.EXE` live in `src/data/viceroy_tables.{h,c}`; see [viceroy_tables.md](viceroy_tables.md). World-map **feature art** (forest/hill/mountain/coast) uses MAPEDIT rules above, not those VICEROY tables.

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
