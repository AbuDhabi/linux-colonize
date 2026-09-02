# Data files vs hardcoded game logic

Development guide: **what to load from original `COLONIZE/` assets** versus **what must be recovered from DOS binaries / decomp / dumps and baked into the Linux port**.

For file-by-file navigation see [original_index.md](original_index.md). Formats and screen wiring: [assets.md](assets.md). Yield / production deep dives: [terrain_yields.md](terrain_yields.md), [building_production.md](building_production.md).

---

## Decision rule (short)

| Prefer | When |
|--------|------|
| **Read at runtime from `COLONIZE/`** | MicroProse shipped the value as an editable catalog, graphic, map, font, sound driver, or save. Altering the file changes DOS behavior. |
| **Extract once → bake into `src/`** | The value lives only inside an `.EXE` (or is pure algorithm). There is no parallel `.TXT` / `.DAT` table. Regenerate via script when possible (`scripts/extract_viceroy_tables.py`). |
| **Never as a runtime dependency** | Memory dumps, decomp C/ASM, RE tool EXEs (`VR_*.EXE`), installer helpers, sample `CONFIG.SYS` text. Use only as evidence while porting. |

When the printed manual / Terrain Chart disagrees with `NAMES.TXT`, **`NAMES.TXT` wins** for numbers. Use the manual for qualitative rules the catalogs omit (town-commons dual produce, SoL ±1, Adam Smith gate, etc.). Community wiki digests ([fandom_col1994.md](fandom_col1994.md) and similar) are **tier-3** evidence only — after original code and the manual — never runtime data.

---

## Artifact roles

| Location | Role |
|----------|------|
| `COLONIZE/` | Shipped game data + EXEs. Intended runtime root next to the Linux executable (`<exe>/COLONIZE`); override with `--data-dir`. Also the default save directory for DOS interop. |
| `original_sources_decompiled/` | Raw Ghidra exports of `VICEROY.EXE` / `MAPEDIT.EXE`. Read-only RE reference; **not** compiled. Do not rename symbols here. |
| `original_sources_annotated/` | Labeled / commented working copy of selected clusters (phase 1: AI accessors + nation turns). Prefer this when a symbol exists; fall back to the raw export. **Not** compiled. |
| `original_memory_dumps/` | DOSBox-X save-states (live VGA / RAM). RE evidence only. |
| `original_saves/` / `test-saves*` | Col1 binary layout oracles — not “game data catalogs”. |
| `src/data/` | Baked tables extracted from EXEs (e.g. `viceroy_tables.c`). |
| `COLONIZE/Colonization.pdf` | Manual + tech supplement — feature intent, not machine tables. |

---

## Part A — Information in `COLONIZE/` data files

### 1. Text catalogs (`.TXT` / `.DB`) — **read from disk**

Sectioned OEM text (`@SECTION`, `;` comments, `~` hotkeys). Parsers already exist (`assets_msg_load_file`).

| File | Contents (load these) |
|------|------------------------|
| **`NAMES.TXT`** | **Primary rules catalog.** Terrain names + MP/defense/improve/value + field yields (`@UNFORESTED` / `@FORESTED` / `@OTHER`); special-resource values (`@RESOURCE`); nations / leaders / ports / difficulty; colonist `@CLASS`; `@BUILDING` hammer/tool/size/min-pop/upkeep; `@SCENARIO` start coords + map names; `@JOB` school tier + Europe hire cost; `@CARGO` market model (start/low/high/burden/rise/fall/…); `@UNIT` icon/moves/combat/cargo/cost/AI role; orders/actions; tribe tech+color (`@TRIBES`); founding-father categories + century weights (`@FATHERS`); UI `@COLORS`. |
| **`GAME.TXT`** | Dialogs, `@BEGINMENU`, options checkboxes, save/load prompts, `@TIMECHANGE` calendar copy, `@PICKMUSIC` (+ Independence/Military/Indian lists), nation intro prose, many in-game messages. |
| **`MENU.TXT`** | Map menu bar sections (`@GAME` `@VIEW` `@ORDERS` `@REPORTS` `@TRADE` `@PEDIA`, optional `@CUP`). |
| **`LABELS.TXT`** | Short UI strings (`@INFO`, customize axes `@MISC`, Europe labels, colony messages). |
| **`COLONY.TXT`** | Colony name pools by nation (founding name picker). |
| **`PEDIA.TXT`** | Colonizopedia article bodies (`@CARGO*` `@UNIT*` `@TERRAIN*` `@JOB*` `@BUILDING*` `@FATHER*` `@MISCELLANEOUS`). |
| **`TRIBE.TXT`** | Historical AMERICA village coordinates per nation (`@IROQUOIS` …). Used for AMERICA / scenario placement, not NEW WORLD procedural tribes. |
| **`OPENING.TXT` / `CLOSING.TXT` / `WOODCUT.TXT`** | Cinematic / woodcut captions. |
| **`MAPEDIT.TXT` / `MAPMENU.TXT`** | Map-editor UI strings (port only if shipping MAPEDIT-like tools). |
| **`ERRORS.DB` / `MODULES.DB`** | Error / module string tables (required at bring-up). |
| **`README.TXT`** | Patch notes (Cathedral min pop 8, Space = free production, etc.) — human guidance, optional to parse. |
| **`MEMORY.TXT` / `AUTOEXEC.TXT` / `CONFIG.TXT`** | DOS install / memory help samples — **do not load as game data**. |
| **`DEBUG.TXT`** | Cheat dialog copy (`@SETVIEW`, `@CREATE`, …). Port loads it for Reveal Map / future cheats; not for MOTD/install samples. |

**Port rule:** Prefer parsing `NAMES.TXT` / `GAME.TXT` / etc. over duplicating string lists or yield grids in C. Several modules still hardcode name arrays as stubs (`reports.c`, `europe.c` port names); migrate those to the catalogs when touching that code.

### 2. Packed graphics — **read from disk**

| Ext | Role | Examples |
|-----|------|----------|
| **`.SS`** | MADSPACK sprite sheets | `TERRAIN.SS`, `PHYS0.SS`, `ICONS.SS`, `CURSOR.SS`, `BUILDING.SS`, `PARCH.SS`, `NAMEPLAT.SS`, `WOODTILE.SS`, `CC-00`…`CC-24.SS`, nation / score / opening / closing sheets |
| **`.PIK`** | MADSPACK full-screen / panel pictures (+ optional palette) | `OPENMENU.PIK`, `WOODPANL.PIK`, `EUROPE.PIK`, `COLONY.PIK`, `CUSTOMIZ.PIK`, `DIFFICUL.PIK`, `NATIONS.PIK`, `REPORT*.PIK`, `CCBKGD.PIK`, `LEVN0001`–`0010.PIK` |
| **`.FF`** | MADSPACK bitmap fonts | `FONTTINY.FF` (primary UI), `FONTSMAL.FF`, `FONTINTR.FF`, `FONTKING.FF`, `FONT-NP.FF` |
| **`.PAL`** | VGA palette | `VICEROY.PAL` (fallback / required check) |

**Port rule:** Never redraw art in code. Decode MADSPACK/FAB; blit with index `0xFD` transparency. Sprite **indices** and compositor **order** come from MAPEDIT/VICEROY logic (baked), not from a text file.

### 3. Maps and scenarios — **read from disk**

| File | Contents |
|------|----------|
| **`AMER2.MP`** | Original Americas 58×72 three-layer map (terrain + flags + fog markers). Critical compositor / AMERICA fixture. |
| Other `*.MP` | Map-editor / scenario maps listed via `@AMERICA` / `@MAPTOLOAD`. |
| **`@SCENARIO` in `NAMES.TXT`** | Per-scenario start tiles for four European powers. |

Procedural NEW WORLD maps are **not** files — generated by VICEROY algorithm (bake; see Part B).

### 4. Sound — **read drivers; bake IDs / timing**

| File | Load? | Notes |
|------|-------|-------|
| **`GSOUND.COL`** | **Yes** | General MIDI voice bytecode (Linux port uses this). |
| `ASOUND.COL` / `PSOUND.COL` / `RSOUND.COL` | Optional later | AdLib / SB / Roland drivers. |
| **`COLDIG.BIN`** | **Yes** | Digital SFX blob — 35 samples, loaded and mixed since 2026-08-27. The old "no reachable playback trigger" note was a decompiler artifact: event ids are passed in `AX`. See [assets.md](assets.md). |
| **`CONFIG.COL`** | Maybe | 20-byte INSTALL sound-card config; Linux picks FluidSynth itself. |
| `GAME.TXT` `@PICKMUSIC` / `@SOUNDOPTIONS` | **Yes** | Song titles and option labels. |

SoundFont for FluidSynth is **not** in `COLONIZE/` (`data/soundfonts/`). Numeric BGM/event IDs (`0x20..`, title `0x33`) and PIT tick rate are **EXE-side** — bake those constants.

### 5. Small / special binaries — mostly **ignore or RE-only**

| File | Verdict |
|------|---------|
| **`AMERICA.MOV`** | Tiny motion/script blob for map tooling — **not** the LEVN voyage cutscene. Skip unless restoring MAPEDIT tooling. |
| **`PATH.DAT`** | CSV of coordinate pairs (~702 lines). Installer / path-drawing helper data — not core gameplay catalogs. |
| **`CYCLE.DAT`** | Water palette-cycle table: uint16 count + 8×[length, phase, start, rate] records (DOS loads it raw to `DS:0x929e`, `FUN_7a9d_0004`). Retail: one cycle, palette 0x78–0x7F, rate 0x23 ticks of the 60.877 Hz `DS:0x92e8` clock (IRQ0 608.77 Hz ÷2 ÷5 in the ISR; ~575 ms/step). Loaded by `game_load_cycle_dat`; `game_water_cycle_tick` rotates the map palette on the map screen when the Water Color Cycling option is on. |
| **`INSTALL.DAT` / `INSTALL.EXE` / `MPSCOPY.EXE` / `PKUNZJR.COM` / `INSTALL.GIF`** | Installer — do not load. |
| **`COLONIZE.BAT` / `COLDEMO.BAT`** | DOS launchers. |
| **`VR_*.EXE` / `VR_SEED.EXE` / `VR_BRAVE*.EXE`** | Local RE / fidelity tools (seed-locked VICEROY variants). Evidence only. |
| **`COLONY##.SAV` in `COLONIZE/`** | Player saves that happen to sit in the data dir — treat like `original_saves/`, not catalogs. |

### 6. Executables shipped with data — **source of baked tables / algorithms**

| EXE | Use for development |
|-----|---------------------|
| **`VICEROY.EXE`** | Main game. Extract static DS tables; decomp for map gen, AI, UI, economy, combat. |
| **`MAPEDIT.EXE`** | Authority for **static map feature art** (coasts, forests, hills, rivers, resource type-by-terrain). |
| **`OPENING.EXE`** | Title intro — **Done** (`src/core/opening.c`). Art `OPENING.PIK` (960×132) + `OPENBORD.PIK`, `OPENSHIP.SS` + `PATH.DAT`, `OPEN{WND1,SUN,MON1,WND2,MON2,MON3,FISH,GUY,LOGO,BONK}.SS`, credits `OPENCRD1/2/3.SS`. Timeline `OPENING.TXT` `@OPENING` / `@CREDITS`. `COLONIZE.BAT` is `opening -g`. Two key/click skip. `skip_intro` in settings.json. |
| **`CLOSING.EXE`** | Rebel-victory outro — **Done** (`src/core/closing.c`). Art `CLOS-BKG.PIK` + `CLOS-{HAT,LDY,MAN,MIL,FWK,ROC,BEL}.SS`; timeline `CLOSING.TXT` `@CLOSING`. VICEROY execs `closing -gok` after `@KINGLOSE`, then CLOSING returns via `viceroy -ow` into the score chain. |

---

## Part B — Information that is hardcoded (EXE / decomp / dumps)

These do **not** appear as editable MicroProse catalogs. Recover from decomp / binary extraction, then **bake** (or regenerate via script).

### 1. Algorithms and control flow

| Domain | Where it lives | Bake into |
|--------|----------------|-----------|
| NEW WORLD map generation (land blobs, climate, rivers, arctic/HS) | VICEROY `FUN_684c_*`, `FUN_67bf_*` | `src/core/map_gen.c` |
| DOS LCG / `range()` | `FUN_1d1d_0e04`, `FUN_19ef_0032` | `src/core/dos_rng.c` |
| Tribe / Brave / satellite placement | `FUN_6a09_*` | `src/core/ai.c` |
| Indian AI / growth / contact | `FUN_4d56_*` | `ai.c` (growth/pulse) + `ai_contact.c` — see [ai_transcription.md](ai_transcription.md) |
| European AI planner | `FUN_521d_*` | `ai_euro.c` / `ai_goals.c` — early T2 slices (seed-100 sail/unload/found) + thin expand/war; `20e6` land arms ported 2026-08-27 (T1.18), deep −0x6790 still open — see [ai_transcription.md](ai_transcription.md) |
| Euro diplomacy | `FUN_15b3_*` / `FUN_5bfb_*` | `ai_diplo.c` — see [ai_transcription.md](ai_transcription.md) |
| King / REF | `FUN_43f7_*` | `ai_king.c` — see [ai_transcription.md](ai_transcription.md) |
| Map tile compositor (masks, draw order) | MAPEDIT `FUN_1a47_*` | `src/core/map.c` |
| Resource / rumour procedural placement | MAPEDIT `FUN_12ab_*` | `map.c` |
| Turn / calendar mechanics beyond `@TIMECHANGE` copy | VICEROY + saves | `src/core/turn.c` |
| Colony manufacturing **tier rates** `3 / 6 / 9` (provisional; DOS via `FUN_15eb_15c6` depth + `FUN_15eb_1d4c`) | Decomp (peel incomplete) + manual Building Chart effects — **not** EXE `@ 0x16103` | `src/core/colony_production.c` |
| Craft recipes (ore→tools, …), SoL ±, shortfall rules | Manual + decomp; not in `NAMES.TXT` | `colony_craft.c` / `colony_production.c` — see [building_production.md](building_production.md), [sons_of_liberty.md](sons_of_liberty.md) |
| Town-commons dual-produce rules | Manual + Col1 fixtures; not a yield-table row | `colony_yield.c` — see [terrain_yields.md](terrain_yields.md) |
| Plow / road / river yield modifiers | `FUN_15eb_18ec` (stacking) | `colony_yield.c` (port still max(road,river) — divergent) |
| UI layout coordinates, hit-rects, wizard image regions | Decomp + DOSBox captures | `new_game.c`, `colony_screen.c`, … |
| Sound ID ranges, ~60 Hz tick, opcode map | Inside `GSOUND.COL` MZ + VICEROY gating | `src/core/sound.c` |
| Col1 save binary layout | Saves + RE | `src/core/col1_save.h` |

### 2. Static tables extractable from EXEs (bake; optional regenerate)

| Table | Source | Linux home |
|-------|--------|------------|
| Terrain meta, tile display, river transition, feature sprite bases, connectivity | `VICEROY.EXE` DS tables | `src/data/viceroy_tables.c` via `scripts/extract_viceroy_tables.py` — see [viceroy_tables.md](viceroy_tables.md) |
| Resource type by terrain class (29 ints) | MAPEDIT DS `0x4de` | Currently hardcoded in `map.c` (`mapedit_resource_type_by_terrain`) — same “extract once” class |
| Manufacturing free-colonist rates `3/6/9` | Port constants; DOS `FUN_15eb_1d4c` / `15eb_15c6` (no valid `0x16103` table) | `colony_production.c` — see [building_production.md](building_production.md) |

**Note:** World-map **feature art** follows **MAPEDIT**, not the VICEROY connectivity tables (those are retained for pedia / residual paths).

### 3. Memory dumps — evidence only

`original_memory_dumps/dosbox_save_state*` provide live `Memory` / `Vga` / `CPU` snapshots (cursor blink, Brave hang, etc.). Use to confirm RAM layouts and timing. **Do not** ship or load dumps at runtime.

### 4. What looks like “data” but is still code

| Temptation | Reality |
|------------|---------|
| Hardcoding yield grids from the manual Terrain Chart | Wrong — use `@UNFORESTED` / `@FORESTED` / `@OTHER` |
| Baking colony names / unit names / FF names as C strings | Prefer `COLONY.TXT` / `NAMES.TXT` / `PEDIA.TXT` |
| Treating `AMERICA.MOV` as the voyage cutscene | Voyage frames are `LEVN*.PIK` + `GAME.TXT` captions |
| Using VICEROY coast tables for the world map | Superseded by MAPEDIT compositor |
| Loading `VR_SEED.EXE` as game content | RE tool only |

---

## Part C — Quick matrix for implementers

| Concern | Source of truth | Runtime approach |
|---------|-----------------|------------------|
| Field yields, building costs, unit stats, cargo prices, FF weights, tribe colors | `NAMES.TXT` | **Parse file** |
| Dialog / menu / music titles / calendar strings | `GAME.TXT`, `MENU.TXT`, `LABELS.TXT` | **Parse file** |
| Pedia prose | `PEDIA.TXT` | **Parse file** |
| AMERICA village sites | `TRIBE.TXT` | **Parse file** |
| Colony name lists | `COLONY.TXT` | **Parse file** |
| Scenario start positions | `NAMES.TXT` `@SCENARIO` | **Parse file** |
| Art, fonts, palettes | `.SS` / `.PIK` / `.FF` / `.PAL` | **Load & decode** |
| Fixed / editor maps | `.MP` | **Load** |
| Music bytecode | `GSOUND.COL` | **Load & interpret** |
| Map gen, RNG, AI, combat formulas | VICEROY decomp | **Bake algorithm** |
| Coast/forest/hill/river blit rules | MAPEDIT decomp | **Bake algorithm** |
| Resource class → type table | MAPEDIT binary | **Bake table** |
| Manufacturing 3/6/9 tiers, class multipliers | Port constants; DOS `15eb_1d4c` peel incomplete | **Bake constants** (cite [building_production.md](building_production.md)) |
| Town commons / plow / road / SoL modifiers | Manual + `15eb_18ec` / SoL docs | **Bake rules** (not in TXT) — see [terrain_yields.md](terrain_yields.md) |
| Screen chrome coordinates | Decomp + dumps | **Bake constants** |
| Save format | `.SAV` + RE | **Bake structs**; R/W files |
| Installer, CONFIG samples, VR tools, dumps | — | **Ignore at runtime** |

---

## Part D — Current port snapshot (guidance, not backlog)

Already **file-driven** in practice: MADSPACK art/fonts, `GAME`/`MENU`/`LABELS`/`NAMES`/`COLONY`/`PEDIA` text, `AMER2.MP` / scenario `.MP`, `TRIBE.TXT` (AMERICA), `GSOUND.COL`, Col1 saves, report PIKs.

Already **baked** (correctly): DOS RNG, map gen pipeline, MAPEDIT compositor + resource-type table, `viceroy_tables.c`, colony production tier math, much UI layout, sound ID map.

Still **duplicated in C** (should migrate toward catalogs when convenient): some report/Europe/job/tribe name arrays; a few nation/port strings in `new_game.c` / `europe.c` that also exist in `NAMES.TXT`.

---

## See also

- [original_index.md](original_index.md) — FUN_* and file index
- [ai_transcription.md](ai_transcription.md) — AI bake inventory and remaining work
- [assets.md](assets.md) — MADSPACK, map layers, UI wiring
- [viceroy_tables.md](viceroy_tables.md) — extracted VICEROY DS tables
- [terrain_yields.md](terrain_yields.md) / [building_production.md](building_production.md) — yield & manufacturing rules
- [decomp_inventory.md](decomp_inventory.md) — bring-up / fidelity status
- [savegame.md](savegame.md) — `COLONY##.SAV` layout
