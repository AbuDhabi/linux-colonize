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
- Audio output

## Bring-Up Strategy Notes

- Do not refactor gameplay logic first.
- Introduce wrappers matching expected legacy behavior.
- Route every platform call through explicit interfaces so unresolved behavior
  can be logged and implemented incrementally.

## Current Linux Bring-Up Status

- SDL2 shell, diagnostics log, save/load POC: done
- `GAME.TXT` / palette / MADSPACK+FAB / `.PIK` decode: done for menu background
- Decomp exports (`viceroy.c`, `viceroy_unpacked.c`) are not compiled into the binary;
  DOS typedef stubs live in `src/platform/dos_compat/dos_types.h` for incremental extraction
- Map compositor lookup tables from `VICEROY.EXE` are extracted to `src/data/viceroy_tables.{h,c}`
  (see `docs/viceroy_tables.md`)
- World map view: terrain + PHYS0 overlays (forests, hills, rivers) — playable bring-up
- **Coastlines and estuaries: parked** — not drawn (`MAP_COAST_OVERLAYS_ENABLED 0`, `MAP_ESTUARY_OVERLAYS_ENABLED 0`); cosmetic only (see **Parked: coastlines and estuaries** below)
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
  mouse cursor moves on map click; unimplemented report/trade/pedia entries stubbed

## Parked: coastlines and estuaries

**Decision (2026-07):** Stop iterating on shoreline and river-mouth art. Wrong rendering does not block gameplay (units, colonies, Europe, turn flow). Revisit only when map fidelity is explicitly prioritized.

### Coast decoration

Coast decoration is **stubbed off** by default: `MAP_COAST_OVERLAYS_ENABLED` in `src/core/map.h` is `0`, so ocean tiles draw TERRAIN only. The parked implementation `map_phys0_coast_collect()` remains in `src/core/map.c` (compiled when the flag is `1`) and blits up to four 8×8 `PHYS0.SS` fragments (sprites 108–139) on ocean tiles using a 3-bit land-neighbour mask per quadrant. Smoke fixtures in `tests/smoke/test_map.c` (`amer2_coast_fixtures`) compile only with the flag enabled. **Do not treat those fixtures as ground truth for DOS fidelity** — they document the heuristic, not the original game.

Land-side shore sprites (140–153) are not drawn. Texture-variation overlays from the DOS precomputed buffers are not implemented.

### River estuaries (ocean + river overlay)

`.MP` marks river mouths as **ocean (index 25) with river overlay** (bits 5–7 = minor or major). The port does **not** draw PHYS0 on those tiles by default (`MAP_ESTUARY_OVERLAYS_ENABLED 0`). Inland rivers (land tiles) are unchanged.

Parked implementation: `phys0_estuary_sprite()` in `src/core/map.c` (compiled when the flag is `1`). It indexed overlay nibble + land-side river neighbour mask against DOS RAM buffer `0x0328f0` (coast fragments 108–139, sprite 149). Looked wrong in-game; folded into this parking lot with coast work. Fixtures: `amer2_river_estuary` in `tests/smoke/test_map.c` (enabled flag only).

### Research already done (keep when resuming)

Full 640 KB conventional RAM from DOSBox-X save state (`dosbox_save_state_2/Memory`; header skip `0x88` bytes before linear RAM):

| Linear addr | Size | Contents |
|-------------|------|----------|
| `0x0324d0` | `map_w × map_h` | Per-tile byte → `PHYS0.SS` sprite index (overlay / variation) |
| `0x0328f0` | `map_w × map_h` | Second per-tile `PHYS0` index buffer (texture variation; not coast direction) |

Findings from correlating AMER2.MP terrain with those buffers:

- Values `0x80`–`0x8A` (sprites 128–138) appear on **both** coast and interior tiles with **no** stable mapping to a 4-neighbour ocean mask — they behave like **random texture variation**, not directional coast indices.
- Open ocean often uses `0x3c`–`0x3e` (sprites 60–62) or `0x00` / `0x44` (no overlay / sprite 68).
- Sprite 149 (`0x95`) appears at river mouths; not wired in the port.
- Unpacked decomp recovered viewport orchestration (`FUN_6a9f_0360`, `FUN_6a9f_0118`), not the leaf that picks coast sprites. RTLink thunks `FUN_210d_0dab` / `FUN_210d_0d91` still gate overlay pages.

`PHYS0.SS` layout (154 sprites): 108–139 = 8×8; 140–153 = 16×16. See [assets.md](assets.md).

Static table `connectivity_transition` at EXE offset `0x5599` (`src/data/viceroy_tables.*`) is a candidate for coast variant selection but was **not** validated against live output. Prior 2×2-corner model (150–153 / 128–131) was replaced by the quadrant heuristic and also looked wrong in-game.

### Checklist when resuming coast / estuary work

0. **Re-enable drawing** — Set `MAP_COAST_OVERLAYS_ENABLED` and/or `MAP_ESTUARY_OVERLAYS_ENABLED` to `1` in `src/core/map.h`; rebuild and compare smoke fixtures.
1. **Ground truth** — Side-by-side DOSBox-X vs Linux screenshot at the same AMER2 view (e.g. cursor ~39,10); note specific wrong tiles.
2. **Breakpoints** — In DOSBox-X, break on blit/write when sprite index ∈ [108,153] and backtrace through loaded overlay (not packed `viceroy.c` stubs). User reports breakpoints were unreliable; save-state RAM diff may be easier.
3. **Buffer consumer** — Find code that **writes** `0x0324d0` / `0x0328f0` (search for stores into those regions, or follow `FUN_6a9f_0118` consumers).
4. **Validate tables** — Compare `connectivity_transition` and overlay nibble paths against live buffer bytes on coast tiles only (filter tiles where land/ocean neighbours differ).
5. **Land-side art** — Add 140–153 (and quadrant placement rules) once ocean-side rules are confirmed.
6. **Tests** — Replace or supplement `tests/smoke/test_map.c` coast fixtures with captures from DOS reference buffers, not hand-derived quadrant math.

### Other map compositor gaps (unchanged priority)

- Texture variation overlays (per-tile random `PHYS0` variants from DOS buffers)
- Roads, resources, fog-of-war
- River/hill connectivity vs `viceroy_tables` (partially heuristic today; inland rivers only)

