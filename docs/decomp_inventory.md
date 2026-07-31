# Decompiled Surface Inventory

This repository keeps Ghidra exports of `VICEROY.EXE` for reverse-engineering
reference. They are not buildable with a modern Linux compiler and retain DOS
memory-model / runtime artifacts.

| File | Source | Notes |
|------|--------|-------|
| `viceroy.c` / `viceroy.asm` | Packed EXE | ~25k / ~139k lines; RTLink overlay pages largely unresolved |
| `viceroy_unpacked.c` / `viceroy_unpacked.asm` | Unpacked EXE | ~125k / ~305k lines; overlay-resident code is present |

Prefer **`viceroy_unpacked.*`** when chasing map-view or overlay call chains.

## High-Level Metrics

- Packed export: ~397 `__cdecl16far` functions
- Unpacked export: substantially larger (overlay bodies + more segments)
- Common synthetic symbols:
  - Globals: `DAT_xxxx_xxxx`
  - Labels: `LAB_xxxx_xxxx`
  - Switch labels: `caseD_*`

## Function Clusters by Segment Prefix

The segment prefix in function names (`FUN_ssss_oooo`) provides a practical
first-pass clustering mechanism:

- `FUN_15eb_*`: high-density logic cluster
- `FUN_1d1d_*`: high-density logic + platform-adjacent routines
- `FUN_1427_*`: mid-size cluster
- `FUN_104b_*`, `FUN_1009_*`: smaller utility/control-flow clusters

This clustering should be preserved in initial source splitting to reduce risk.

## DOS/Hardware-Coupled Surfaces

Observed direct I/O and hardware assumptions in the decomp exports:

- VGA DAC and retrace interaction:
  - `out(0x3c8, ...)`
  - `out(..., 0x3c9)`
  - `in(0x3da)`
- PIT timer programming:
  - `out(0x43, 0x36)`
  - `out(0x40, ...)`
- Conventional VGA memory segment assumptions:
  - references involving `0xa000`
- BIOS tick/global data style references:
  - patterns around `DAT_0000_046c` and low-memory globals

These routines belong behind a Linux platform API and must not remain as raw
port I/O in the native build.

## Proposed Boundary: Core vs Platform

### Core Candidate (kept behavior-first)

- Game state updates
- Turn progression and simulation
- Economic/unit/map logic
- Scenario/rules logic

### Platform Candidate (replace with SDL2/Linux services)

- Palette and framebuffer presentation
- Keyboard/mouse polling and event translation
- Time/tick services
- File path, save location, and case normalization
- Audio output (SDL callback + FluidSynth; see `src/core/sound.c`)

## Bring-Up Strategy Notes

- Do not refactor gameplay logic first.
- Introduce wrappers matching expected legacy behavior.
- Route every platform call through explicit interfaces so unresolved behavior
  can be logged and implemented incrementally.

## Current Linux Bring-Up Status

- SDL2 shell, diagnostics log, save/load: original `COLONY##.SAV` structs +
  byte-identical I/O in `src/core/col1_save.{h,c}`; runtime bridge in
  `src/core/col1_bridge.c` (see `docs/savegame.md`); verified against
  `original_saves/COLONY00.SAV` / `COLONY01.SAV`

- `GAME.TXT` / palette / MADSPACK+FAB / `.PIK` decode: done for menu background
- Decomp exports (`viceroy.c`, `viceroy_unpacked.c`) are not compiled into the binary;
  DOS typedef stubs live in `src/platform/dos_compat/dos_types.h` for incremental extraction
- Map compositor lookup tables from `VICEROY.EXE` are extracted to `src/data/viceroy_tables.{h,c}`
  (see `docs/viceroy_tables.md`)
- World map view: terrain + PHYS0 overlays (forests, hills, rivers, coasts, estuaries)
- Coast / estuary: enabled from `MAPEDIT.EXE` compositor (`FUN_1a47_0932`; see below)
- **Music playback: parked** — `COLONIZE_SOUND_PLAYBACK_ENABLED 0` in `src/core/sound.h`; loader kept (see `docs/assets.md`)
- Europe screen bring-up: `EUROPE.PIK` + market quotes / dock recruit from `NAMES.TXT`
  (press **E** from the map; see `src/core/europe.c`)
- Colony screen bring-up: `WOODPANL.PIK` + `PARCH.SS` (full buildings section) +
  `WOODTILE.SS` (square minimap panel) + `BUILDING.SS` starters / tree placeholders
  (sprites 42–47) + top bar (name/date/gold) + 1px black separators + `COLONY.PIK` bottom
  panel + `ICONS.SS` #22–37 cargo strip (icon + amount) + population stub + centered 3×3
  surroundings with PHYS0 (press **C** on a colony tile; see `src/core/colony_screen.c`)
- Units bring-up: `@UNIT` types from `NAMES.TXT`, map icons from `ICONS.SS`,
  starter Pioneer + Caravel, select/move, deploy dock immigrants (**D**),
  board/unload (**O**/**U**), sail ship to/from Europe with passengers
  (**H** on high seas / **S** in Europe; see `src/core/units.c`, `src/core/europe.c`)
- Map menu bar: `MENU.TXT` pull-downs with mouse hit-testing (`src/core/map_menu.c`);
  mouse cursor moves on map click; Colonizopedia category lists from `PEDIA.TXT`
  (`src/core/pedia.c`); pull-down divider after Terrain Types; trade menu entries stubbed
- Report / adviser screens: F2–F10 + REPORTS menu (`src/core/reports.c`);
  F1 Terrain Information → Colonizopedia at cursor; F8=`REPORT8.PIK`; F10=`WOODPANL.PIK`;
  F2–F9 filled from Col1 save + runtime pools (crosses, FF, labor, trade, warehouses,
  ships, rivals, tribes); F10 Colonization Score from manual schedule
  (`reports_compute_score`)
- Colonizopedia: woodcut list screen (`WOODPANL.PIK`) with green entry links in up to
  3 columns, then cargo/unit/terrain/job/building/father/misc articles with
  ICONS / BUILDING / CC-NN / TERRAIN previews
- Turn progression (`src/core/turn.c`): `@TIMECHANGE` calendar, colony production
  stub, nation crosses/bells hooks, EN→FR→SP→DU / Indian / King phase stubs,
  Wait-for-next-unit, End of Turn option, autosave hooks (slots 9 / 8),
  turn-owner indicator (`FUN_1984_00aa`: 5×3 at 315,197; shown only during AI/Indian
  EOT phases; `@COUNTRY` / `@TRIBES` colors)
- Music (`src/core/sound.c`): **parked** — `COLONIZE_SOUND_PLAYBACK_ENABLED 0`;
  `GSOUND.COL` loader/ID map kept; heuristic MIDI playback disabled until fidelity work;
  `COLDIG.BIN` SFX still deferred

## End-of-turn recovery checklist

Ordered pipeline recovered for the Linux port (human-centric; AI actions are stubs):

1. **Human ends turn** — Space / ORDERS → No Orders (`LABELS.TXT` “End of Turn”)
2. **Advance calendar** — `head.year` / `autumn` / `turn` (`@TIMECHANGE` in `GAME.TXT`):
   one turn/year until 1600; thereafter Spring then Autumn each year
3. **Colony production** — food ± (produce 3 / consume 2 per colonist), lumber/ore stubs,
   hammers toward `building_in_production` (Colony Space cheat = production only;
   `README.TXT` “free turn”)
4. **Nation ticks** — liberty bells + crosses; crosses ≥ needed → dock immigrant;
   founding-father election **not** recovered yet
5. **European AI** — EN→FR→SP→DU via `player.control` (0 human / 1 AI / 2 withdrawn);
   currently refresh MP only
6. **Indians** — refresh native unit MP (`nation_id` 4..11); raids deferred
7. **King** — stub (tax / REF / independence events deferred)
8. **Refresh human MP** + select next unit with moves (“Continue turn.”)

Evidence:

| Source | Finding |
|--------|---------|
| `GAME.TXT` `@TIMECHANGE` | Biannual seasons from 1600 |
| `original_saves/COLONY00/01.SAV` | turn 0→2 ≈ year 1492→1494 (1 year/turn) |
| `README.TXT` | Dutch turn ends European order; colony Space = free production |
| `LABELS.TXT` | “End of Turn” / “Continue turn.” |
| `NAMES.TXT` `@COUNTRY` / `@TRIBES` | Turn-owner box colors (DS:0x848 / 0x84c) |
| `FUN_1984_00aa` / `FUN_281f_0590` | 5×3 fill at (0x13b, 0xc5) overlaid on screen |
| `viceroy_unpacked.asm` | `TIMECHANGE` / `MULTINEXT` / `SEASONS` string table only (no FUN_* XREF yet) |

AI production for non-human Europeans is **skipped** until save-diff evidence says otherwise; human colonies always tick.

## Map coastlines and estuaries (MAPEDIT)

Authoritative static map compositor: `COLONIZE/MAPEDIT.EXE` / `mapedit.c` (`FUN_1a47_0932`, land mask `FUN_1a47_01ae`). No RTLink; no fog-of-war / animation. Enabled by default (`MAP_COAST_OVERLAYS_ENABLED` / `MAP_ESTUARY_OVERLAYS_ENABLED` in `src/core/map.h`).

### Coast decoration

On ocean / high-seas tiles with at least one land neighbour:

1. Build 8-bit land mask (N→NW clockwise) and four 3-bit quadrant masks (cardinal → bits 0/2 on adjacent quads; diagonal → bit 1).
2. Special full-tile corners when mask matches (id 0..3 = land NW/NE/SW/SE):
   PHYS0 **`150 + id`**. MAPEDIT encodes `0x97+id` (151–154); the sheet is
   150–153 in land-direction order (transparent on the land side of the tile).
3. Else four 8×8 fragments: sprite **`109 + 4*quad_mask + q`** at pixel offsets NW/NE/SE/SW (`0`/`8`).

Draw order vs MAPEDIT: land TERRAIN underlayer (last cardinal neighbour) → coast PHYS0 →
masked ocean into palette-0 holes (`FUN_1a47_0676`) → estuary. Fog of war is not drawn
(MAPEDIT skips it too).

### River estuaries

Ocean tile with `terrain & 0xc0`: for each cardinal neighbour that is land with `terrain & 0x40`, blit **`141+q`** (major, bit `0x80` set) or **`145+q`** (minor) as 16×16 at `(0,0)`. Inland rivers unchanged.

Fixtures: `amer2_coast_fixtures` / `amer2_river_estuary` in `tests/smoke/test_map.c`.

### Remaining map compositor gaps

- Fog-of-war / exploration blackness (intentional MAPEDIT gap; game still needs it)
- Coast animation frames; texture-variation overlays from DOS RAM buffers
- Prior VICEROY RAM-buffer / parked quadrant heuristics — superseded by MAPEDIT

### Other map compositor gaps (unchanged priority)

- Texture variation overlays (per-tile random `PHYS0` variants from DOS buffers)
- Roads, resources, fog-of-war
- River/hill connectivity vs `viceroy_tables` (partially heuristic today; inland rivers only)

