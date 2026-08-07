# Module map — segment → system

Cheat sheet for Ghidra segment prefixes (`FUN_<seg>_<off>`). Per-function detail: [`FUNCTION_CATALOG.md`](FUNCTION_CATALOG.md). Deep AI labels: [`SYMBOL_MAP.md`](SYMBOL_MAP.md).

Confidence here is for the **segment cluster**, not every function. Unlabeled segments stay `unknown` until a catalog peel (layer A) assigns a tag. Function seeds may override a segment tag (e.g. `281f` thunks, `1427`→ai) — that is intentional, not a bug.

## Progress

**VICEROY:** 2380 funcs · confidence known=88 inferred=2289 unknown=3 · system unknown=3 · segments 164 labeled / 2 unknown (of 166).

Remaining unknown segments (2): `205f` `1d1c`

**MAPEDIT (parked):** 557 funcs · confidence known=19 inferred=413 unknown=125 · system unknown=125 · segments 24 labeled / 91 unknown (of 115).

## VICEROY

| Segment | Defs | System | Confidence | Cluster label | Catalog |
|---------|-----:|--------|------------|---------------|---------|
| `281f` | 371 | thunk | inferred | Far thunks: RNG / map / UI fill helpers | [catalog](FUNCTION_CATALOG.md) |
| `2a1f` | 294 | mapgen | inferred | Map-gen dispatch / helpers (also Euro act thunks) | [catalog](FUNCTION_CATALOG.md) |
| `291f` | 271 | thunk | inferred | Far thunks: EMS page-in then overlay JMPF (2f2b/38fd/6f74/...) | [catalog](FUNCTION_CATALOG.md) |
| `1d1d` | 127 | platform | inferred | High-density + platform-adjacent (incl. LCG) | [catalog](FUNCTION_CATALOG.md) |
| `210d` | 116 | platform | inferred | DOS/EMS runtime: INT 21/67, bank switch, overlay page helpers | [catalog](FUNCTION_CATALOG.md) |
| `15eb` | 107 | mapdraw | inferred | High-density map / pedia draw paths | [catalog](FUNCTION_CATALOG.md) |
| `38fd` | 81 | trade | inferred | Nation Europe market / cargo trade (nation*0x13c via 0x84fc) | [catalog](FUNCTION_CATALOG.md) |
| `2f2b` | 75 | colony | inferred | Colony screen / build / colonist logic (DS:0x8542) | [catalog](FUNCTION_CATALOG.md) |
| `6f74` | 58 | ui | known | Text layout / flow-wrap dialog compositor (incl. FUN_6f74_1198) | [catalog](FUNCTION_CATALOG.md) |
| `1427` | 55 | mapdraw | inferred | Tile / unit display and MP chrome | [catalog](FUNCTION_CATALOG.md) |
| `2b5a` | 53 | ui | inferred | Map selected-unit order / input UI (DS:0x5392) | [catalog](FUNCTION_CATALOG.md) |
| `104b` | 29 | ui | inferred | Text / number blit helpers (1d1d_11b4) | [catalog](FUNCTION_CATALOG.md) |
| `521d` | 29 | ai | known | European AI planner | [catalog](FUNCTION_CATALOG.md) |
| `137f` | 26 | mapgen | inferred | Map plane accessors (terrain/layer2/3) | [catalog](FUNCTION_CATALOG.md) |
| `4b58` | 24 | ui | inferred | Window / frame widget draw (281f_00ba family) | [catalog](FUNCTION_CATALOG.md) |
| `647e` | 23 | colony | inferred | Colony list / select UI (rec*0x4a via DS:0x9e14) | [catalog](FUNCTION_CATALOG.md) |
| `275d` | 21 | platform | inferred | DOS PATH/env parse / memory sizing / INT21 helpers | [catalog](FUNCTION_CATALOG.md) |
| `43f7` | 21 | ui | known | Nation / @COUNTRY colors | [catalog](FUNCTION_CATALOG.md) |
| `4d56` | 21 | ai | known | Indian AI / village growth | [catalog](FUNCTION_CATALOG.md) |
| `6cb2` | 21 | ui | inferred | Info / dialog text panels (281f compositor) | [catalog](FUNCTION_CATALOG.md) |
| `75c2` | 20 | save | known | Savegame R/W of units / colonies / tribes / flags | [catalog](FUNCTION_CATALOG.md) |
| `1984` | 18 | turn | known | Turn-owner chrome | [catalog](FUNCTION_CATALOG.md) |
| `1a58` | 18 | platform | known | Mouse driver INT 33 show/hide / poll / mode setup | [catalog](FUNCTION_CATALOG.md) |
| `3f41` | 18 | ui | inferred | Report / diplomacy / market UI screens | [catalog](FUNCTION_CATALOG.md) |
| `6ba1` | 18 | mapdraw | inferred | Map tile neighbor masks / viewport blit helpers | [catalog](FUNCTION_CATALOG.md) |
| `1009` | 15 | ui | inferred | Timed turn-chrome / status text overlays | [catalog](FUNCTION_CATALOG.md) |
| `4345` | 13 | trade | inferred | Nation trade flags / FF / Europe-market helpers | [catalog](FUNCTION_CATALOG.md) |
| `479b` | 13 | colony | inferred | Pioneer clear/plow / goto-colony order bodies | [catalog](FUNCTION_CATALOG.md) |
| `5bfb` | 12 | ai | inferred | Indian contact / diplomacy / alarm | [catalog](FUNCTION_CATALOG.md) |
| `733a` | 12 | ui | known | New-game / CUSTOMIZE UI | [catalog](FUNCTION_CATALOG.md) |
| `364b` | 11 | colony | inferred | Colony found / build screen (DS:0x8542) | [catalog](FUNCTION_CATALOG.md) |
| `5fef` | 11 | combat | inferred | Unit/colony combat and Indian raid resolution | [catalog](FUNCTION_CATALOG.md) |
| `1262` | 10 | ui | inferred | Input wait / mouse hit-test / tip overlays | [catalog](FUNCTION_CATALOG.md) |
| `15b3` | 10 | trade | inferred | Nation bilateral flags / name tables (Euro 0x13c, Indian 0x4e) | [catalog](FUNCTION_CATALOG.md) |
| `41f2` | 9 | ai | known | Tribe growth (Indian-turn growth tick + message UI) | [catalog](FUNCTION_CATALOG.md) |
| `48d3` | 9 | ai | inferred | Euro landfall goto / unit-order helpers | [catalog](FUNCTION_CATALOG.md) |
| `49dd` | 9 | ui | inferred | Unit cargo / profession status panels | [catalog](FUNCTION_CATALOG.md) |
| `6b22` | 9 | mapdraw | inferred | Map viewport tribe/colony/unit overlay blit | [catalog](FUNCTION_CATALOG.md) |
| `7314` | 9 | platform | inferred | Config/name file line parse (comma fields) | [catalog](FUNCTION_CATALOG.md) |
| `112b` | 8 | mapdraw | known | Unit orders/allegiance chrome (FUN_112b_01ba) | [catalog](FUNCTION_CATALOG.md) |
| `684c` | 8 | mapgen | known | Procedural NEW WORLD map gen | [catalog](FUNCTION_CATALOG.md) |
| `6a9f` | 8 | mapdraw | known | Map viewport tile loop | [catalog](FUNCTION_CATALOG.md) |
| `1097` | 7 | ui | inferred | Multi-item dialog spacing / number layout | [catalog](FUNCTION_CATALOG.md) |
| `124c` | 7 | platform | inferred | Small helpers (e.g. DOS distance) | [catalog](FUNCTION_CATALOG.md) |
| `129f` | 7 | sound | known | BGM helpers | [catalog](FUNCTION_CATALOG.md) |
| `4cc6` | 7 | ai | inferred | Indian tribe relations / nearest-village / contact score | [catalog](FUNCTION_CATALOG.md) |
| `6662` | 7 | ui | inferred | Goto pathfinding BFS + path-cost overlay | [catalog](FUNCTION_CATALOG.md) |
| `6afa` | 7 | mapdraw | inferred | Map viewport clamp / tile blit helpers | [catalog](FUNCTION_CATALOG.md) |
| `7455` | 7 | mapgen | inferred | Map plane buffer alloc (pitch 0x853a → terrain/L2/L3) | [catalog](FUNCTION_CATALOG.md) |
| `1101` | 6 | ui | inferred | 16-row glyph/bitmap blit helpers | [catalog](FUNCTION_CATALOG.md) |
| `5952` | 6 | colony | inferred | Colony production / buildings / stock tick (DS:0x8542) | [catalog](FUNCTION_CATALOG.md) |
| `130d` | 5 | turn | known | Main game year/turn loop + intro splash | [catalog](FUNCTION_CATALOG.md) |
| `13f1` | 5 | mapdraw | inferred | Exploration-bit / fog reveal around units & colonies | [catalog](FUNCTION_CATALOG.md) |
| `15dc` | 5 | ai | known | Tribe / Indian current-context setters & lookups | [catalog](FUNCTION_CATALOG.md) |
| `2047` | 5 | platform | known | DOS Ctrl-C/Break / INT21 abort handlers | [catalog](FUNCTION_CATALOG.md) |
| `6b7e` | 5 | mapdraw | inferred | Map viewport refresh / camera save-restore | [catalog](FUNCTION_CATALOG.md) |
| `7562` | 5 | save | known | COLONY## slot path / list / Save(0-7) / Load(0-9) / autosave | [catalog](FUNCTION_CATALOG.md) |
| `7a05` | 5 | platform | inferred | Fatal/abort error text + INT10 video reset | [catalog](FUNCTION_CATALOG.md) |
| `7ada` | 5 | platform | known | DOS heap alloc / resize / high-water tracking | [catalog](FUNCTION_CATALOG.md) |
| `7b08` | 5 | platform | inferred | Growable far-buffer / arena alloc helpers | [catalog](FUNCTION_CATALOG.md) |
| `13e4` | 4 | mapgen | inferred | Terrain class / ocean helpers | [catalog](FUNCTION_CATALOG.md) |
| `19ef` | 4 | platform | known | DOS LCG range helper | [catalog](FUNCTION_CATALOG.md) |
| `19f6` | 4 | ui | inferred | Decimal number format / localized string blit | [catalog](FUNCTION_CATALOG.md) |
| `1a29` | 4 | platform | known | DOS timer INT vector install / restore | [catalog](FUNCTION_CATALOG.md) |
| `1acb` | 4 | ui | inferred | Mouse hit-rect / button edge tracking | [catalog](FUNCTION_CATALOG.md) |
| `478c` | 4 | colony | inferred | Colonist (type 0x17) / ship unit spawn helpers | [catalog](FUNCTION_CATALOG.md) |
| `4962` | 4 | ai | inferred | Per-nation unit/colony/cargo census tallies | [catalog](FUNCTION_CATALOG.md) |
| `78d8` | 4 | platform | inferred | Resource stream buffer alloc / cursor / far-ptr load | [catalog](FUNCTION_CATALOG.md) |
| `79a8` | 4 | platform | inferred | Compressed resource stream I/O + progress callback | [catalog](FUNCTION_CATALOG.md) |
| `12fd` | 3 | ui | inferred | Once-only discovery/event dispatch (bitset DS:0x540a) | [catalog](FUNCTION_CATALOG.md) |
| `157e` | 3 | combat | inferred | Unit combat strength / engagement modifiers | [catalog](FUNCTION_CATALOG.md) |
| `1a0a` | 3 | ui | inferred | VGA page-flip / palette-cycle animation | [catalog](FUNCTION_CATALOG.md) |
| `1c0c` | 3 | platform | known | Timer / tick word readers (custom + BIOS 046c) | [catalog](FUNCTION_CATALOG.md) |
| `2059` | 3 | sound | known | Sound driver jump table | [catalog](FUNCTION_CATALOG.md) |
| `3844` | 3 | turn | inferred | Euro EOT treasure / ship-ready unit chrome | [catalog](FUNCTION_CATALOG.md) |
| `4720` | 3 | ui | inferred | Ship embark / naval-move validity + order UI (DS:0x9e4e) | [catalog](FUNCTION_CATALOG.md) |
| `5f7a` | 3 | trade | inferred | Colony native-trade / cargo sell & buy | [catalog](FUNCTION_CATALOG.md) |
| `6f30` | 3 | ui | inferred | Splash / image load+blit via resource stream | [catalog](FUNCTION_CATALOG.md) |
| `74a4` | 3 | ui | inferred | Map menu bar load from MENU.TXT (4b58 widgets) | [catalog](FUNCTION_CATALOG.md) |
| `7962` | 3 | platform | inferred | Resource file open / close handle helpers | [catalog](FUNCTION_CATALOG.md) |
| `7a65` | 3 | ui | inferred | Map tip blit + parameterized dialog text (6f74) | [catalog](FUNCTION_CATALOG.md) |
| `1000` | 2 | ui | inferred | String-table index / Nth-string lookup | [catalog](FUNCTION_CATALOG.md) |
| `12dd` | 2 | ui | inferred | Clipped blit dispatch (rect vs raw) | [catalog](FUNCTION_CATALOG.md) |
| `12e9` | 2 | ui | inferred | Buffer fill via pitch helpers | [catalog](FUNCTION_CATALOG.md) |
| `1a4e` | 2 | ui | inferred | Blit pitch offset + viewport rect clip | [catalog](FUNCTION_CATALOG.md) |
| `1ae3` | 2 | platform | known | Stack clear + BIOS INT16 key-ready | [catalog](FUNCTION_CATALOG.md) |
| `1ae7` | 2 | platform | known | BIOS INT16 keyboard read / queue flush | [catalog](FUNCTION_CATALOG.md) |
| `1afb` | 2 | platform | inferred | String LF-terminate / NUL truncate | [catalog](FUNCTION_CATALOG.md) |
| `1b2c` | 2 | platform | inferred | DOS char-stream string get / put | [catalog](FUNCTION_CATALOG.md) |
| `1b32` | 2 | platform | inferred | Filename extension / basename helpers | [catalog](FUNCTION_CATALOG.md) |
| `1b70` | 2 | ui | inferred | Mouse viewport / region setup (1a58) | [catalog](FUNCTION_CATALOG.md) |
| `1bdd` | 2 | platform | inferred | Temp numbered file create / write slots | [catalog](FUNCTION_CATALOG.md) |
| `1c2e` | 2 | ui | known | VGA vsync wait + DAC palette write | [catalog](FUNCTION_CATALOG.md) |
| `1d11` | 2 | ui | known | INT10 mode set + Mode13h far-buffer blit | [catalog](FUNCTION_CATALOG.md) |
| `205f` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2088` | 2 | platform | known | XMS detect (INT2F) + init handle table | [catalog](FUNCTION_CATALOG.md) |
| `2103` | 2 | platform | known | XMS handle alloc / free list | [catalog](FUNCTION_CATALOG.md) |
| `465b` | 2 | combat | inferred | Move spent / ADD / combat-adjacent | [catalog](FUNCTION_CATALOG.md) |
| `67f4` | 2 | mapgen | inferred | Coast/neighbor bitmasks + continent tallies | [catalog](FUNCTION_CATALOG.md) |
| `7421` | 2 | platform | inferred | Startup config fread + argv parse | [catalog](FUNCTION_CATALOG.md) |
| `79ec` | 2 | platform | inferred | Resource stream progress pump / dispatch | [catalog](FUNCTION_CATALOG.md) |
| `7a4c` | 2 | ui | inferred | VGA DAC write + PIT fade-speed calibrate | [catalog](FUNCTION_CATALOG.md) |
| `7a83` | 2 | ui | inferred | Palette RGB fade / channel shift | [catalog](FUNCTION_CATALOG.md) |
| `7aa1` | 2 | ui | inferred | Parameterized dialog / message box (# subst) | [catalog](FUNCTION_CATALOG.md) |
| `7ab9` | 2 | ui | inferred | Image/resource load + blit error codes | [catalog](FUNCTION_CATALOG.md) |
| `7acf` | 2 | platform | inferred | Far-buffer alloc into handle struct | [catalog](FUNCTION_CATALOG.md) |
| `7b04` | 2 | platform | known | DOS free-memory probe (INT21) + size max | [catalog](FUNCTION_CATALOG.md) |
| `12d6` | 1 | ui | inferred | Mouse-gated blit to VGA A000 | [catalog](FUNCTION_CATALOG.md) |
| `12d8` | 1 | sound | known | BGM / event / SFX gating | [catalog](FUNCTION_CATALOG.md) |
| `1ade` | 1 | ui | known | VGA vsync + DAC palette write | [catalog](FUNCTION_CATALOG.md) |
| `1aea` | 1 | ui | inferred | Map keyboard / hotkey dispatch | [catalog](FUNCTION_CATALOG.md) |
| `1b01` | 1 | platform | inferred | Buffered far-buffer file/stream read | [catalog](FUNCTION_CATALOG.md) |
| `1b22` | 1 | platform | known | DOS file-exists probe (INT21 3D/3E) + INT24 wrap | [catalog](FUNCTION_CATALOG.md) |
| `1b4e` | 1 | platform | inferred | Path join: cwd + \ + name | [catalog](FUNCTION_CATALOG.md) |
| `1b57` | 1 | platform | inferred | Ltrim spaces/tabs then strcpy | [catalog](FUNCTION_CATALOG.md) |
| `1b5e` | 1 | ui | inferred | Mouse cursor region update (17x17) | [catalog](FUNCTION_CATALOG.md) |
| `1b78` | 1 | ui | inferred | Mouse cursor region update (16x16) | [catalog](FUNCTION_CATALOG.md) |
| `1b8b` | 1 | platform | known | Set BIOS equipment video-mode bits (40:10) | [catalog](FUNCTION_CATALOG.md) |
| `1b8d` | 1 | ui | inferred | Solid-rect fill thunk → 1b9e | [catalog](FUNCTION_CATALOG.md) |
| `1b8f` | 1 | ui | inferred | Pitched buffer rect blit/copy | [catalog](FUNCTION_CATALOG.md) |
| `1b9e` | 1 | ui | known | Solid-color pitched rect fill | [catalog](FUNCTION_CATALOG.md) |
| `1baa` | 1 | ui | inferred | Pitched buffer rect blit/copy (variant) | [catalog](FUNCTION_CATALOG.md) |
| `1bb9` | 1 | ui | known | Put pixel via pitch helper (1a4e) | [catalog](FUNCTION_CATALOG.md) |
| `1bbb` | 1 | ui | known | Get pixel via pitch helper (1a4e) | [catalog](FUNCTION_CATALOG.md) |
| `1bbc` | 1 | ui | known | Horizontal span fill in pitched buffer | [catalog](FUNCTION_CATALOG.md) |
| `1bc3` | 1 | ui | known | Vertical span fill in pitched buffer | [catalog](FUNCTION_CATALOG.md) |
| `1bca` | 1 | ui | known | Rect outline via H/V span fills | [catalog](FUNCTION_CATALOG.md) |
| `1bd4` | 1 | ui | inferred | Color replace in pitched rect | [catalog](FUNCTION_CATALOG.md) |
| `1bf5` | 1 | ui | inferred | Tiled rect blit loop | [catalog](FUNCTION_CATALOG.md) |
| `1c05` | 1 | platform | known | Normalize far pointer (seg:off) | [catalog](FUNCTION_CATALOG.md) |
| `1c06` | 1 | platform | inferred | Parse 0x/0b numeric literal prefix | [catalog](FUNCTION_CATALOG.md) |
| `1c10` | 1 | platform | known | Program PIT channel-0 divisor | [catalog](FUNCTION_CATALOG.md) |
| `1c11` | 1 | ui | inferred | 2-bit packed glyph decode to pitched buffer | [catalog](FUNCTION_CATALOG.md) |
| `1c28` | 1 | ui | inferred | Store text-draw color words at DS:269e | [catalog](FUNCTION_CATALOG.md) |
| `1c2a` | 1 | ui | inferred | String pixel-width via glyph table | [catalog](FUNCTION_CATALOG.md) |
| `1c36` | 1 | ui | inferred | Soft-sprite / cursor RLE blit | [catalog](FUNCTION_CATALOG.md) |
| `1c56` | 1 | ui | inferred | Scaled/dithered sprite blit | [catalog](FUNCTION_CATALOG.md) |
| `1c83` | 1 | ui | inferred | Sprite scale to destination size/pos | [catalog](FUNCTION_CATALOG.md) |
| `1c89` | 1 | ui | inferred | Soft-sprite blit (sibling of 1c36) | [catalog](FUNCTION_CATALOG.md) |
| `1caa` | 1 | ui | inferred | Scaled sprite blit (sibling of 1c56) | [catalog](FUNCTION_CATALOG.md) |
| `1cd8` | 1 | ui | inferred | Soft-sprite blit (3rd sibling) | [catalog](FUNCTION_CATALOG.md) |
| `1cf8` | 1 | platform | inferred | Insertion-sort parallel word+byte arrays | [catalog](FUNCTION_CATALOG.md) |
| `1d05` | 1 | platform | inferred | Insertion-sort parallel byte+byte arrays | [catalog](FUNCTION_CATALOG.md) |
| `1d1c` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `206d` | 1 | platform | inferred | Stream buffer fill + far-ptr normalize | [catalog](FUNCTION_CATALOG.md) |
| `2074` | 1 | platform | inferred | Size-budgeted stream read to 1b01 | [catalog](FUNCTION_CATALOG.md) |
| `2094` | 1 | platform | inferred | Rtrim trailing spaces/tabs | [catalog](FUNCTION_CATALOG.md) |
| `209a` | 1 | platform | known | Parse hex integer from string | [catalog](FUNCTION_CATALOG.md) |
| `20a0` | 1 | platform | known | Parse binary integer from string | [catalog](FUNCTION_CATALOG.md) |
| `2100` | 1 | platform | known | XMS size query via multiplex entry | [catalog](FUNCTION_CATALOG.md) |
| `3f3f` | 1 | platform | known | CRC/LFSR step | [catalog](FUNCTION_CATALOG.md) |
| `636c` | 1 | ui | inferred | Dual-column compare / report dialog | [catalog](FUNCTION_CATALOG.md) |
| `65dd` | 1 | combat | inferred | Unit combat outcome resolution (RNG cases) | [catalog](FUNCTION_CATALOG.md) |
| `67bf` | 1 | mapgen | known | Continent flood-fill IDs | [catalog](FUNCTION_CATALOG.md) |
| `682a` | 1 | mapgen | known | Map fertility / bonus value writer | [catalog](FUNCTION_CATALOG.md) |
| `6a09` | 1 | ai | known | Tribe placement | [catalog](FUNCTION_CATALOG.md) |
| `78ef` | 1 | platform | inferred | Resource archive open (RM* / ext) + stream setup | [catalog](FUNCTION_CATALOG.md) |
| `7939` | 1 | platform | inferred | Resource open helper (path + close) | [catalog](FUNCTION_CATALOG.md) |
| `7944` | 1 | platform | inferred | Resource open helper (sibling) | [catalog](FUNCTION_CATALOG.md) |
| `7952` | 1 | platform | inferred | Resource open / alloc path | [catalog](FUNCTION_CATALOG.md) |
| `798d` | 1 | platform | inferred | Compressed resource chunk read | [catalog](FUNCTION_CATALOG.md) |
| `79db` | 1 | platform | inferred | Chunked DOS file read into far buffer | [catalog](FUNCTION_CATALOG.md) |
| `7a7c` | 1 | ui | inferred | Load 768-byte VGA palette from resource | [catalog](FUNCTION_CATALOG.md) |
| `7a9d` | 1 | ui | inferred | Dialog string buffer prep (0x929e) | [catalog](FUNCTION_CATALOG.md) |
| `7ab3` | 1 | ui | known | VGA vsync + DAC palette read | [catalog](FUNCTION_CATALOG.md) |
| `7ad6` | 1 | platform | inferred | Far-buffer free / clear handle struct | [catalog](FUNCTION_CATALOG.md) |

## MAPEDIT (separate EXE)

| Segment | Defs | System | Confidence | Cluster label | Catalog |
|---------|-----:|--------|------------|---------------|---------|
| `2388` | 98 | platform | known | DOS CRT / runtime: INT21 I/O, env parse, strlen/strcpy family | [catalog](FUNCTION_CATALOG.md) |
| `1000` | 61 | ui | inferred | MAPEDIT main UI hub: menus, viewport, terrain chrome | [catalog](FUNCTION_CATALOG.md) |
| `133d` | 56 | ui | inferred | Dialog / window layout and compositor | [catalog](FUNCTION_CATALOG.md) |
| `18ad` | 28 | ui | inferred | Text / number blit and string-table helpers | [catalog](FUNCTION_CATALOG.md) |
| `12ab` | 26 | mapdraw | known | Resources / rumours | [catalog](FUNCTION_CATALOG.md) |
| `16d7` | 24 | ui | inferred | Text layout / flow-wrap compositor (~ color switches) | [catalog](FUNCTION_CATALOG.md) |
| `1f65` | 23 | platform | known | Mouse driver INT 33 show/hide / init / poll | [catalog](FUNCTION_CATALOG.md) |
| `1a47` | 17 | mapdraw | known | Tile compositor entry and land/coast/transition/river/hill/forest masks | [catalog](FUNCTION_CATALOG.md) |
| `2074` | 10 | ui | inferred | VGA palette RGB match / slot allocator | [catalog](FUNCTION_CATALOG.md) |
| `1b56` | 9 | mapdraw | inferred | Minimap terrain palette cache + viewport blit | [catalog](FUNCTION_CATALOG.md) |
| `1cc9` | 9 | platform | known | DOS conventional heap (MCB INT21) | [catalog](FUNCTION_CATALOG.md) |
| `1842` | 8 | platform | inferred | Config/text file open + line/comma-field parse | [catalog](FUNCTION_CATALOG.md) |
| `19f9` | 8 | mapdraw | inferred | Map plane buffer alloc / .MP load-save / clear | [catalog](FUNCTION_CATALOG.md) |
| `1865` | 7 | platform | inferred | Clamp / swap / approx distance / facing helpers | [catalog](FUNCTION_CATALOG.md) |
| `130b` | 6 | mapdraw | known | 16x16 sprite blit + color-0 mask blit | [catalog](FUNCTION_CATALOG.md) |
| `187b` | 6 | ui | inferred | Viewport rect clip and blit helpers | [catalog](FUNCTION_CATALOG.md) |
| `212d` | 6 | platform | known | EMS INT67 page map + INT21 env helpers | [catalog](FUNCTION_CATALOG.md) |
| `1f45` | 5 | platform | inferred | Growable far-buffer / arena on 1cc9 | [catalog](FUNCTION_CATALOG.md) |
| `2145` | 5 | platform | inferred | EMS page-frame heap / handle allocator | [catalog](FUNCTION_CATALOG.md) |
| `1297` | 4 | platform | inferred | Path/name helpers: digit overlay, *-strip copy, fopen | [catalog](FUNCTION_CATALOG.md) |
| `19b7` | 4 | mapdraw | known | Terrain class index | [catalog](FUNCTION_CATALOG.md) |
| `1ed0` | 4 | platform | inferred | Fatal/abort error text + INT10 video reset | [catalog](FUNCTION_CATALOG.md) |
| `2115` | 4 | platform | known | DOS Ctrl-C/Break / INT21 abort handlers | [catalog](FUNCTION_CATALOG.md) |
| `2309` | 4 | platform | inferred | Compressed resource stream I/O + progress callback | [catalog](FUNCTION_CATALOG.md) |
| `1334` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `18a2` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c21` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d08` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d1c` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `221a` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `22b0` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `18f9` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1baf` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bc4` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c04` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c34` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c3e` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c91` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c9d` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d18` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d75` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1ec5` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1f16` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1f3e` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2025` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `203d` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2061` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2127` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2202` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2281` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `228b` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `19c4` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1ba3` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1baa` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bb2` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bca` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bea` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bfb` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c11` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c1a` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c3c` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c46` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c49` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c4c` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c5b` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c67` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c76` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c78` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c79` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c80` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c86` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1cb9` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d12` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d43` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d53` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d6a` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d6c` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d70` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d8f` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1dae` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1ddb` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1e71` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1e92` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1ec0` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1f36` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1ffe` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `201f` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `204f` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2050` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2056` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `205b` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `205c` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2069` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2114` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2258` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `227b` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `227e` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2289` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2290` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2292` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2297` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `22aa` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `22c7` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `22e0` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `22ec` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `22fb` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2302` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `233e` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2343` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2345` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2357` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2359` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2360` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2367` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2378` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |

## Peel protocol

One thin layer per session (see annotated README). Do not mix layers:

| Layer | Work |
|-------|------|
| **A** | Segment labeling — assign system to next unlabeled high-def segment |
| **B** | String/XREF pass — label functions from `.asm` strings for one game area |
| **C** | Call-tree from known entry — label one-hop callees |
| **D** | Selective deepen — extract annotated stub under `original_sources_annotated/<system>/` |

