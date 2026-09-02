# Original Sources and Data Index

**First stop** when looking up original DOS Colonization behavior: which artifact to
open, how to find a `FUN_*`, what each data file family is for, and which doc owns
the deep detail.

| Topic | Deep dive |
|-------|-----------|
| **Linux code architecture** (present + intended) | [architecture.md](architecture.md) |
| **Decomp function catalog** (all `FUN_*`, light) | [`../original_sources_annotated/FUNCTION_CATALOG.md`](../original_sources_annotated/FUNCTION_CATALOG.md) · [`MODULE_MAP.md`](../original_sources_annotated/MODULE_MAP.md) |
| **Catalog peel ranking** (what to label next) | [catalog_peel_ranking.md](catalog_peel_ranking.md) |
| **Data files vs bake-into-code** (dev guide) | [data_vs_hardcoded.md](data_vs_hardcoded.md) |
| Manual vs port feature gaps | [manual_gap.md](manual_gap.md) |
| Move-into-tile authority (enter / landfall) | [move_enter.md](move_enter.md) |
| Combat mechanics (odds / peels / outcomes / coastal fort) | [combat.md](combat.md) |
| Unit orders (issue / tick / gates / port status) | [unit_orders.md](unit_orders.md) |
| Project goals / acceptance order | [project_goals.md](project_goals.md) |
| **Whole-project roadmap** (phases / what’s next) | [roadmap.md](roadmap.md) |
| Bring-up status, EOT pipeline, map fidelity gaps | [decomp_inventory.md](decomp_inventory.md) |
| **Between player turns** (full EOT map + Layer D `130d`/`3844`) | [turn_between_players.md](turn_between_players.md) |
| **AI transcription gap** (Euro / Indian FUN_*, roadmap) | [ai_transcription.md](ai_transcription.md) |
| Formats, UI wiring, map draw order, sound | [assets.md](assets.md) |
| Terrain / field / town-commons yields | [terrain_yields.md](terrain_yields.md) |
| Settlement building production / skills | [building_production.md](building_production.md) |
| Difficulty level effects (0 Discoverer … 4 Viceroy) | [difficulty.md](difficulty.md) |
| Sons of Liberty / rebel sentiment | [sons_of_liberty.md](sons_of_liberty.md) |
| **Indians** (graphics / units / settlements / alarm / contact) | [indians.md](indians.md) |
| Fandom wiki digest (1994 Col only; tier-3) | [fandom_col1994.md](fandom_col1994.md) |
| Extracted VICEROY DS tables | [viceroy_tables.md](viceroy_tables.md) |
| `COLONY##.SAV` layout / Col1 bridge | [savegame.md](savegame.md) |
| Col1 opaque field atlas / RE roadmap | [save_format_map.md](save_format_map.md) |
| Seed-100 Brave / early-AI fidelity | [seed100_brave.md](seed100_brave.md) |

This file is a **navigation layer**. It does not re-copy compositor algorithms,
MADSPACK layouts, or full bring-up checklists.

---

## How to use

1. Identify the subsystem (map gen, colony screen, AI, sound, …).
2. Look up the `FUN_*` (or its segment) in the light catalog:
   [`MODULE_MAP.md`](../original_sources_annotated/MODULE_MAP.md) →
   [`FUNCTION_CATALOG.md`](../original_sources_annotated/FUNCTION_CATALOG.md).
3. If the symbol has a **deep** extract (AI today), prefer
   [`original_sources_annotated/`](../original_sources_annotated/)
   ([`SYMBOL_MAP.md`](../original_sources_annotated/SYMBOL_MAP.md)).
4. Otherwise open the raw export under
   [`original_sources_decompiled/`](../original_sources_decompiled/)
   (`viceroy_unpacked.c` / `.asm` or `mapedit.c`).
5. Match the Linux module under `src/core/` and the data file under `COLONIZE/`.
6. Follow the deep-dive link for formats and port status.

Regenerate the catalog after re-export: `python3 scripts/gen_fun_catalog.py`.

---

## Decompiled sources at a glance

Ghidra exports live in [`original_sources_decompiled/`](../original_sources_decompiled/).
Not buildable; DOS memory-model / runtime artifacts remain. The Linux binary never
compiles these files.

| Artifact | Source | Size (approx.) | When to use |
|----------|--------|----------------|-------------|
| [`FUNCTION_CATALOG.md`](../original_sources_annotated/FUNCTION_CATALOG.md) / [`MODULE_MAP.md`](../original_sources_annotated/MODULE_MAP.md) | Light labels for **all** `FUN_*` | ~2380 VICEROY + ~557 MAPEDIT | **First stop** for any unknown symbol / segment |
| [`original_sources_annotated/`](../original_sources_annotated/) `ai/` + [`SYMBOL_MAP.md`](../original_sources_annotated/SYMBOL_MAP.md) | Deep AI / accessor slices | growing | Prefer when the symbol is deeply mapped |
| [`viceroy_unpacked.c`](../original_sources_decompiled/viceroy_unpacked.c) / [`.asm`](../original_sources_decompiled/viceroy_unpacked.asm) | Unpacked `VICEROY.EXE` | ~125k / ~305k lines | Raw export — full game logic, RTLink overlays, map gen, UI |
| [`mapedit.c`](../original_sources_decompiled/mapedit.c) | `MAPEDIT.EXE` | ~23k lines | Static world-map **feature art** (coasts, transitions, forest/hill/river masks) |
| [`gsound.c`](../original_sources_decompiled/gsound.c) / [`.asm`](../original_sources_decompiled/gsound.asm) | `GSOUND.COL` | ~2.3k / ~43k lines | GM MIDI driver + voice bytecode interpreter |
| `COLONIZE/VICEROY.EXE` | Shipped binary | ~483 KB | Table extraction, file byte offsets (`scripts/extract_viceroy_tables.py`) |
| `COLONIZE/MAPEDIT.EXE` | Shipped binary | — | Authority for static map compositor rules |

**Address-space warning:** VICEROY and MAPEDIT are **separate** programs. Never equate
a MAPEDIT `FUN_1a47_*` with any VICEROY `FUN_*` of the same digits. The packed
`viceroy.c` export was removed (overlay bodies unresolved); always use
`viceroy_unpacked.*`.

---

## Looking up `FUN_*` / `DAT_*`

### Naming

| Pattern | Meaning |
|---------|---------|
| `FUN_<seg>_<off>` | Function at segment:offset (Ghidra synthetic) |
| `DAT_<seg>_<off>` | Global / static data |
| `LAB_*`, `caseD_*` | Labels / switch cases |

Calling convention in the C export is usually `__cdecl16far` (16-bit far); MAPEDIT
compositor helpers are often `__cdecl16near` (same-segment).

Prefer **`.c`** for reading control flow; prefer **`.asm`** for XREFs and embedded
string literals (`SAVEGAME`, `EUROPE`, `PICKMUSIC`, …).

### Cookbook

```bash
# Function body in VICEROY unpacked C
rg -n '__cdecl16.*FUN_684c_08c0\(' original_sources_decompiled/viceroy_unpacked.c

# Call sites / XREFs in asm
rg -n 'FUN_684c_08c0' original_sources_decompiled/viceroy_unpacked.asm

# MAPEDIT-only (separate address space)
rg -n '__cdecl16.*FUN_1a47_0932' original_sources_decompiled/mapedit.c

# Strings that hint at a subsystem
rg -n 'PICKMUSIC\|CUSTOMIZ\|SAVEGAME' original_sources_decompiled/viceroy_unpacked.asm
```

Start with the **segment prefix** (`684c` = map-gen cluster), then the offset within
that segment.

### Segment-prefix cheat sheet

**Full table** (every segment with def counts): [`MODULE_MAP.md`](../original_sources_annotated/MODULE_MAP.md).
High-value known prefixes:

| Prefix | Cluster (known use) |
|--------|---------------------|
| `FUN_684c_*` | Procedural NEW WORLD map gen |
| `FUN_67bf_*` | Continent flood-fill IDs |
| `FUN_733a_*` | New-game / CUSTOMIZE UI |
| `FUN_2a1f_*` | Map-gen dispatch / helpers |
| `FUN_281f_*` | RNG / small UI fill helpers |
| `FUN_1984_*` | Turn-owner chrome |
| `FUN_4d56_*` | Indian AI / village growth |
| `FUN_6a09_*` | Tribe placement |
| `FUN_521d_*` | European AI planner |
| `FUN_6a9f_*` | Map viewport tile loop |
| `FUN_15eb_*` | High-density logic (pedia / map draw paths) |
| `FUN_1d1d_*` | High-density + platform-adjacent |
| `FUN_1427_*` | Tile display helpers |
| `FUN_12d8_*` / `FUN_2059_*` / `FUN_129f_*` | Sound / BGM gating and drivers |
| `FUN_43f7_*` | King/REF/tax/independence + `@COUNTRY` colors |
| `FUN_1a47_*` (MAPEDIT) | Tile compositor / coast / transitions |
| `FUN_12ab_*` (MAPEDIT) | Resources / rumours |
| `FUN_19b7_*` (MAPEDIT) | Terrain class index |

Full bring-up narrative and fidelity notes → [decomp_inventory.md](decomp_inventory.md).

---

## Known entry-point index

High-value addresses already cited in this repo. “Linux” is the port counterpart when
one exists.

### VICEROY (`original_sources_decompiled/viceroy_unpacked.c`)

| Address | Purpose | Linux / docs |
|---------|---------|--------------|
| `FUN_2a1f_083e` | Dispatches into map-gen pipeline | [map_gen.c](../src/core/map_gen.c), [assets.md](assets.md) |
| `FUN_684c_08c0` | NEW WORLD procedural map entry | `map_generate` / `MapGenParams` |
| `FUN_684c_02a8` / `0116` / `021c` | Land blobs / form thunks | map_gen |
| `FUN_67bf_0000` | Continent flood-fill IDs | map_gen (IDs not written to layer2 in v1) |
| `FUN_733a_0000` / `0270` / `0512` | CUSTOMIZE / difficulty-style UI | [new_game.c](../src/core/new_game.c) |
| `FUN_281f_04d4` | Wrapped RNG (calls into libc) | `dos_rng` (`FUN_1d1d_0e04`) |
| `FUN_1d1d_0e04` / `FUN_19ef_0032` | DOS LCG + range | [dos_rng.c](../src/core/dos_rng.c); golden via `golden_mapgen_seed100` |
| `FUN_281f_0590` | Fill helper (turn box) | turn indicator draw |
| `FUN_1984_00aa` | Nation turn-owner 5×3 at (315,197) | [turn.c](../src/core/turn.c) |
| `FUN_43f7_05f4` | `@COUNTRY` → DS color table | turn / UI colors |
| `FUN_4d56_152e` | Indian village growth | [ai.c](../src/core/ai.c) (partial); [ai_transcription.md](ai_transcription.md) |
| `FUN_4d56_1816` | Indian nation turn | **partial** (structural phases + quiet pulse + `ai_contact_*`; `4528` Done 2026-08-27/28, `2820` rewritten 2026-08-29; deep `2820` haggle / VGA PARKED) |
| `FUN_6a09_0006` | Tribe placement | ai / map gen (T2 seed-100) |
| `FUN_521d_6d8e` | Euro AI dispatcher | **partial** (`ai_euro.c` skeleton + `ai_euro_early_turn` / `golden_ai_turns`) |
| `FUN_521d_0a60` / `5d04` | Euro unit goals / planning | **partial** (`0a60` goal-consumption tail + `5d04` hire ladder ported; T3.1 closed 2026-08-27) |
| `FUN_521d_20e6` / nested `5b66` | Move scoring / unit act | **partial** (land arms structurally ported 2026-08-27, T1.18; thin: LAB_52aa attack odds, explore-plane seen nibble, `−0x6168`, `0x4c` village arms) |
| `FUN_6a9f_0118` | Map viewport tile loop | [map.c](../src/core/map.c) / map_panel |
| `FUN_15eb_06d2` | Shared world-map / pedia draw entry | map / pedia |
| `FUN_1427_065a` | Tile display (reads DS `0x5234`) | [viceroy_tables.md](viceroy_tables.md) |
| `FUN_12d8_000e` | BGM / event / SFX gating | [sound.c](../src/core/sound.c) |
| `FUN_2059_000a` | Sound driver jump table | sound.c |
| `FUN_129f_*` | BGM helpers (e.g. `0008`, `00f6`, `0300`) | sound.c |

### GSOUND (`original_sources_decompiled/gsound.c`)

| Symbol | Role | Linux / notes |
|--------|------|---------------|
| `FUN_1000_01fd` | Voice opcode interpreter | [sound.c](../src/core/sound.c) decode |
| `FUN_1000_19bc` | Sound-ID → handler tables | BGM `0x2A6E`, event `0x2AC4` |
| — | Opcode / tempo notes | [gsound_interpreter.md](../original_sources_annotated/sound/gsound_interpreter.md) |

### MAPEDIT (`original_sources_decompiled/mapedit.c`)

| Address | Purpose | Linux / docs |
|---------|---------|--------------|
| `FUN_1a47_0932` | Tile draw / compositor entry | [map.c](../src/core/map.c), [assets.md](assets.md), inventory |
| `FUN_1a47_01ae` | Land mask / coast setup | map.c |
| `FUN_1a47_05b2` | Coastal underlayer | map.c |
| `FUN_1a47_0676` | Masked ocean fill | map.c |
| `FUN_1a47_06da` | Land–land transitions | map.c |
| `FUN_1a47_030e` / `036e` / `0418` | River / hill / forest connectivity masks | map.c |
| `FUN_12ab_0458` / `0540` / `0380` / `0204` | Resources / rumours | map.c / assets |
| `FUN_19b7_0006` | Terrain class index | map / pedia |

---

## `COLONIZE/` data index

Runtime data root (~289 files). Intended location at runtime: `COLONIZE/` next to
the Linux executable (same pile as DOS). Override with `--data-dir`. Formats and
screen wiring: [assets.md](assets.md). Official 3.0 notes: `COLONIZE/README.TXT`.

### Extension counts

| Ext | ~N | Role |
|-----|---:|------|
| `.SS` | 206 | MADSPACK sprite sheets |
| `.PIK` | 35 | MADSPACK full-screen / panel pictures |
| `.TXT` | 18 | UI / dialog / catalog text (`@SECTION`) |
| `.EXE` | 6 | Game + tools |
| `.FF` | 5 | MADSPACK bitmap fonts |
| `.COL` | 5 | Sound drivers + INSTALL card config |
| `.DB` | 2 | String tables |
| `.MP` | 1 | Scenario map |
| `.BIN` | 1 | Digital SFX |
| `.DAT` / `.BAT` / `.PAL` / `.MOV` / … | few | Install, launch, palette, motion blob |

### Maps

| File | Purpose |
|------|---------|
| `AMER2.MP` | Original Americas 58×72 three-layer map — **critical** for compositor bring-up |

### Core sprites (high priority among `.SS`)

| File | Purpose |
|------|---------|
| `TERRAIN.SS` | Base terrain tiles |
| `PHYS0.SS` | Rivers, hills, mountains, forest, coast, resources |
| `CURSOR.SS` | Map cursor; #0 used as OS pointer |
| `ICONS.SS` | Cargo / unit / hold icons |
| `NAMEPLAT.SS` | Woodcut caption plate: 3 sprites (left cap 18px / middle tile 16px / right cap 18px), tiled to the caption width — `FUN_6f30_0062` |
| `BUILDING.SS` / `BDARK.SS` | Colony buildings |
| `PARCH.SS` | Colony buildings parchment |
| `WOODTILE.SS` | Wood fill (menu bar, panel, in-game popups) |
| `OPENTILE.SS` | Title-menu popup fill |
| `WOODFRAM.SS` | Woodcut picture frame, 274x170, anchor (160,184) — `FUN_6f30_0062`, not the colony screen |

### Bulk `.SS` prefixes (lower priority for day-to-day port work)

| Prefix / set | ~N | Purpose |
|--------------|---:|---------|
| `DEC*` | ~53 | Declaration / revolution art |
| `IND*` | ~32 | Native nation art (`IND0A0`…`IND7A3`, 8×4); meet/VGA chrome — see [indians.md](indians.md) |
| `CC-00`…`CC-24` | 25 | Founding Fathers portraits |
| `SCORE*` | ~24 | Score / fame |
| `OPEN*` | ~15 | Title / opening pieces |
| `WDCUT*` | 13 | Woodcut illustrations, `WDCUT%02d.SS` by milestone id — [popups.md](popups.md) §11 |
| `CLOS-*` | ~7 | Closing cinematic |
| Nation (`ENGLND` / `FRANCE` / `DUTCH` / `SPAIN`) | 8 | Nation art |

### Pictures `.PIK` (by screen cluster)

| Cluster | Examples | Purpose |
|---------|----------|---------|
| Title / wizard | `OPENMENU.PIK`, `DIFFICUL.PIK`, `NATIONS.PIK`, `CUSTOMIZ.PIK`, `LEVN0001`–`0010.PIK` | Menu, difficulty, nation, customize, voyage |
| Game chrome | `WOODPANL.PIK`, `WOODPAN2.PIK`, `EUROPE.PIK`, `COLONY.PIK` | Colony / Europe / wood panels |
| Reports | `REPORT1`–`9.PIK`, `CCBKGD.PIK` | Adviser plates / Congress |
| Misc | `DECOIND.PIK` + `DEC-UPP*/DEC-LOW*/DEC-SQIG.SS`, `CLOS-BKG.PIK`, `KINGLSS*.PIK` | Declaration signing cinematic (`FUN_43f7_160a`), closing, king loss. `DECLARAT.PIK` ships but **no executable references it** — unused leftover |

### Text catalogs

| File | Purpose |
|------|---------|
| `GAME.TXT` | Dialogs, `@BEGINMENU`, options, `@PICKMUSIC` |
| `MENU.TXT` | Map menu bar sections |
| `NAMES.TXT` | Units, cargo, buildings, `@COLORS` — **heavily used** |
| `LABELS.TXT` | Short UI labels |
| `COLONY.TXT` | Colony name lists by nation |
| `PEDIA.TXT` | Colonizopedia bodies |
| `TRIBE.TXT` | Tribe dispersal (map gen / historical) |
| `MAPEDIT.TXT` / `MAPMENU.TXT` | Map editor UI strings |
| `OPENING.TXT` / `CLOSING.TXT` / `WOODCUT.TXT` | Cinematic / woodcut captions |
| `ERRORS.DB` / `MODULES.DB` | Error / module string tables |

### Fonts `.FF`

| File | Typical use in port |
|------|---------------------|
| `FONTTINY.FF` | Menu bar, panel, pedia lists (**primary**) |
| `FONTSMAL.FF` | Small UI |
| `FONTINTR.FF` | Intro / name entry |
| `FONTKING.FF` | King screens |
| `FONT-NP.FF` | Nameplate-style |

### Sound / motion

| File | Purpose |
|------|---------|
| `GSOUND.COL` | General MIDI driver — **used by Linux port** |
| `ASOUND.COL` / `PSOUND.COL` / `RSOUND.COL` | AdLib / SB / Roland (not used yet) |
| `COLDIG.BIN` | Digital SFX — decoded and played (2026-08-27); see [assets.md](assets.md) |
| `AMERICA.MOV` | Short map-tooling motion blob (not the LEVN voyage) |
| `CONFIG.COL` | INSTALL sound-card config |

Repo SoundFont (not in `COLONIZE/`): `data/soundfonts/Roland_SC-55.sf2`.

### Executables / launch

| File | Purpose |
|------|---------|
| `VICEROY.EXE` | Main game — decomp + table extraction |
| `MAPEDIT.EXE` | Map editor — static map art rules |
| `OPENING.EXE` | Intro player (not ported) |
| `CLOSING.EXE` | Rebel-victory outro — ported as `src/core/closing.c` |
| `INSTALL.EXE` / `MPSCOPY.EXE` | Installer / copy |
| `COLONIZE.BAT` / `COLDEMO.BAT` | Launchers |

### Critical for the Linux port (short list)

`AMER2.MP`, `TERRAIN.SS`, `PHYS0.SS`, `CURSOR.SS`, `ICONS.SS`, `WOODTILE.SS`,
`OPENTILE.SS`, `WOODPANL.PIK`, `OPENMENU.PIK`, `FONTTINY.FF`, `GAME.TXT`, `MENU.TXT`,
`NAMES.TXT`, `LABELS.TXT`, `COLONY.TXT`, `PEDIA.TXT`, `GSOUND.COL`, `EUROPE.PIK`,
`PARCH.SS`, `BUILDING.SS`, `NAMEPLAT.SS`, report / `CCBKGD` / wizard PIKs, `CC-*.SS`,
plus RE binaries `VICEROY.EXE` / `MAPEDIT.EXE`.

---

## DOSBox and memory artifacts

Undeclared RE captures of a running `VICEROY` under DOSBox-X live under
[`original_memory_dumps/`](../original_memory_dumps/). Use when decomp + static
data are not enough (live VGA, RAM layouts, cursor blink timing).

### Save-state folders

| Path | Remark | Timestamp |
|------|--------|-----------|
| [`original_memory_dumps/dosbox_save_state/`](../original_memory_dumps/dosbox_save_state/) | `colonization` | 2026-07-29 10:54 |
| [`original_memory_dumps/dosbox_save_state_2/`](../original_memory_dumps/dosbox_save_state_2/) | `while_map_cursor_blinking` | 2026-07-29 11:23 |
| [`original_memory_dumps/dosbox_save_state_brave/`](../original_memory_dumps/dosbox_save_state_brave/) | seed-100 Brave hang (`brave`) | — |

Both early expanded folders: DOSBox-X 2026.07.02 (SDL2), `Program_Name` = `VICEROY`,
`Machine_Type` = `MCH_VGA`, configured `Memory_Size` = 4096 (KB).

### Per-file roles (expanded folders)

| File | Role |
|------|------|
| `Memory` | Full guest RAM image (~16.3 MiB including header/overhead) |
| `CPU` | CPU core / register state |
| `Vga` | VGA memory + registers |
| `Dos` / `EMS` / `XMS` / `DMA` / `Pic` / … | DOS and hardware subsystem state |
| `Mixer` / `Midi` | Audio |
| `Mouse` / `Keyboard` / `Joystick` | Input |
| `Save_Remark` / `Time_Stamp` / `Program_Name` / `DOSBox-X_Version` / `Machine_Type` / `Memory_Size` | Text metadata |

**Difference between `dosbox_save_state` and `_2`:** later capture (~29 minutes) during
map-cursor blink. Large blobs (`Memory`, `CPU`, `Vga`, …) differ; several small device
dumps and all version/program metadata match. Useful pair for “what changes while the
tile cursor blinks.” Prefer `Memory` for a full guest-RAM snapshot.

### Original saves

| Path | Purpose |
|------|---------|
| [`original_saves/COLONY00.SAV`](../original_saves/) / `COLONY01.SAV` | Sample Col1 saves for load fallback and RE |

Layout and bridge → [savegame.md](savegame.md). Field atlas / RE roadmap →
[save_format_map.md](save_format_map.md).

---

## Related tooling and fixtures

| Path | Purpose |
|------|---------|
| `COLONIZE/Colonization.pdf` | Official manual + tech supplement — feature gaps in [manual_gap.md](manual_gap.md) |
| `scripts/extract_viceroy_tables.py` | Pull static tables from `VICEROY.EXE` → `src/data/viceroy_tables.c` ([viceroy_tables.md](viceroy_tables.md)) |
| `scripts/gen_fun_catalog.py` | Regenerate light `FUN_*` catalog + module map ([FUNCTION_CATALOG.md](../original_sources_annotated/FUNCTION_CATALOG.md)) |
| `data/soundfonts/` | FluidSynth bank for `GSOUND.COL` playback |
| `test-assets*`, `test-saves*` | Minimal TXT/DB and save fixtures for automated tests |
| `src/platform/dos_compat/` | DOS typedef stubs for incremental extraction — not a full runtime |

---

## Quick “where do I look?” matrix

| Question | Start here |
|----------|------------|
| Should this value come from a data file or C? | [data_vs_hardcoded.md](data_vs_hardcoded.md) |
| Where is this `FUN_*`? | [FUNCTION_CATALOG.md](../original_sources_annotated/FUNCTION_CATALOG.md) / [MODULE_MAP.md](../original_sources_annotated/MODULE_MAP.md) |
| How does NEW WORLD map gen work? | `FUN_684c_08c0` in `original_sources_decompiled/viceroy_unpacked.c` → `src/core/map_gen.c` |
| Why does coast/forest art look wrong? | `original_sources_decompiled/mapedit.c` `FUN_1a47_*` → `src/core/map.c` + [assets.md](assets.md) |
| What does this `.PIK` / `.SS` decode as? | [assets.md](assets.md) |
| Save file field / nation gold? | [savegame.md](savegame.md) + [save_format_map.md](save_format_map.md) + `original_saves/` |
| Live palette / blink timing? | `original_memory_dumps/dosbox_save_state_2/` (`Vga` / `Memory`) |
| Is this feature already ported? | [decomp_inventory.md](decomp_inventory.md) bring-up list |
