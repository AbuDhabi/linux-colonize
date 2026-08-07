# Module map — segment → system

Cheat sheet for Ghidra segment prefixes (`FUN_<seg>_<off>`). Per-function detail: [`FUNCTION_CATALOG.md`](FUNCTION_CATALOG.md). Deep AI labels: [`SYMBOL_MAP.md`](SYMBOL_MAP.md).

Confidence here is for the **segment cluster**, not every function. Unlabeled segments stay `unknown` until a catalog peel (layer A) assigns a tag.

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
| `7562` | 5 | save | inferred | Hall of Fame / score-file format & list UI | [catalog](FUNCTION_CATALOG.md) |
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
| `78d8` | 4 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `79a8` | 4 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `12fd` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `157e` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1a0a` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c0c` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2059` | 3 | sound | known | Sound driver jump table | [catalog](FUNCTION_CATALOG.md) |
| `3844` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `4720` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `5f7a` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `6f30` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `74a4` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7962` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7a65` | 3 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1000` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `12dd` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `12e9` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1a4e` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1ae3` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1ae7` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1afb` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b2c` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b32` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b70` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bdd` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c2e` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d11` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `205f` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2088` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2103` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `465b` | 2 | combat | inferred | Move spent / ADD / combat-adjacent | [catalog](FUNCTION_CATALOG.md) |
| `67f4` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7421` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `79ec` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7a4c` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7a83` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7aa1` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7ab9` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7acf` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7b04` | 2 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `12d6` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `12d8` | 1 | sound | known | BGM / event / SFX gating | [catalog](FUNCTION_CATALOG.md) |
| `1ade` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1aea` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b01` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b22` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b4e` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b57` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b5e` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b78` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b8b` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b8d` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b8f` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b9e` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1baa` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bb9` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bbb` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bbc` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bc3` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bca` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bd4` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1bf5` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c05` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c06` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c10` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c11` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c28` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c2a` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c36` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c56` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c83` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1c89` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1caa` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1cd8` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1cf8` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d05` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1d1c` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `206d` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2074` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2094` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `209a` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `20a0` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2100` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `3f3f` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `636c` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `65dd` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `67bf` | 1 | mapgen | known | Continent flood-fill IDs | [catalog](FUNCTION_CATALOG.md) |
| `682a` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `6a09` | 1 | ai | known | Tribe placement | [catalog](FUNCTION_CATALOG.md) |
| `78ef` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7939` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7944` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7952` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `798d` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `79db` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7a7c` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7a9d` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7ab3` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `7ad6` | 1 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |

## MAPEDIT (separate EXE)

| Segment | Defs | System | Confidence | Cluster label | Catalog |
|---------|-----:|--------|------------|---------------|---------|
| `2388` | 98 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1000` | 61 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `133d` | 56 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `18ad` | 28 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `12ab` | 26 | mapdraw | known | Resources / rumours | [catalog](FUNCTION_CATALOG.md) |
| `16d7` | 24 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1f65` | 23 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1a47` | 17 | mapdraw | known | Tile compositor entry and land/coast/transition/river/hill/forest masks | [catalog](FUNCTION_CATALOG.md) |
| `2074` | 10 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1b56` | 9 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1cc9` | 9 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1842` | 8 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `19f9` | 8 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1865` | 7 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `130b` | 6 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `187b` | 6 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `212d` | 6 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1f45` | 5 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2145` | 5 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `1297` | 4 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `19b7` | 4 | mapdraw | known | Terrain class index | [catalog](FUNCTION_CATALOG.md) |
| `1ed0` | 4 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2115` | 4 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
| `2309` | 4 | unknown | unknown | — | [catalog](FUNCTION_CATALOG.md) |
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

