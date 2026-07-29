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
- World map view: terrain + partial PHYS0 overlays (coasts, some forests/hills/rivers) —
  good enough for bring-up; see **Deferred: map compositor** below (unpacked decomp
  recovered viewport code, not PHYS0 sprite selection)
- Europe screen bring-up: `EUROPE.PIK` + market quotes / dock recruit from `NAMES.TXT`
  (press **E** from the map; see `src/core/europe.c`)

## Deferred: map compositor

Map rendering is intentionally parked. The Linux path in `src/core/map.c` is
fixture-driven and incomplete (missing coast edge bands 112–127 / 132–139,
animation 140–147, generalized forests, roads/resources/fog, accurate river/hill
connectivity).

### Call chain (packed vs unpacked)

World-map refresh still enters through **`FUN_281f_*`** stubs (e.g.
`FUN_281f_0e38` from `FUN_1984_010a`). Those symbols remain thin RTLink
thunks via `FUN_210d_0dab` / `FUN_210d_0d91` (“smart vectoring”; `VICEROY.EXE`
contains the `RTLink` / `call to Vector … in Page …` strings).

In the **packed** exports (`viceroy.c` / `viceroy.asm`), the far jump targets
inside overlay pages were never loaded — segment `CODE_99` / `0x281f` is mostly
stubs and `??` bytes.

In the **unpacked** exports, those thunks resolve to real overlay functions:

| Symbol | Role (as recovered) |
|--------|---------------------|
| `FUN_6a9f_0360` | Viewport clip + refresh orchestrator (~56×39 tiles) |
| `FUN_6a9f_0118` | Per-tile loop: read map layers (`FUN_137f_*` via thunks), write **one display byte** per cell using tables `0xa576` / `0x848` |
| `FUN_6b22_04bc` | Unit markers over the viewport |
| `FUN_137f_*` | Map-cell address / layer getters (real bodies) |
| `FUN_157e_004a` | Unit art: `0x5235` / `0x5236` × 8 → sprite index (not terrain coasts) |

So unpacking recovered **map viewport orchestration**, not the TERRAIN.SS /
PHYS0.SS **sprite compositor**.

### What is still missing

- No decompiled neighbor-mask → PHYS0 index path for coasts **112–153**
  (or forests / hills / rivers as drawn on the world map).
- DS table `0x5599` (`connectivity_transition`) still absent from the C export
  (BSS / runtime-initialized in the packed DATA listing).
- `0x54de` (`river_transition`) appears in unit/UI-ish paths (e.g. around
  `FUN_112b_010e`), not in `FUN_6a9f_0118`.
- `FUN_6a9f_0118` fills an attribute/color tile buffer; the leaf that blits
  TERRAIN/PHYS0 sprites from those (or related) values is not yet identified.

### When revisiting map fidelity

1. Work from **`viceroy_unpacked.*`**, starting at `FUN_6a9f_0360` /
   `FUN_6a9f_0118`, and find who consumes the per-tile buffer (or who calls
   SS blit with PHYS0 indices).
2. DOSBox-break on PHYS0 blit with sprite IDs 112–153 and backtrace into the
   loaded image — still the most direct way to name the leaf compositor.

Until then: keep existing smoke fixtures green; do not expand coast heuristics
unless a user-reported tile is clearly wrong for playability. Static tables in
`src/data/viceroy_tables.*` still need validation against that leaf once found.

