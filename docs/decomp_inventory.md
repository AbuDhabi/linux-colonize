# Decompiled Surface Inventory

This repository currently has a single large decompiled source file: `viceroy.c`.
The decompilation is not directly buildable with a modern Linux compiler and
contains DOS memory-model and runtime artifacts that require adaptation.

## High-Level Metrics

- Approximate decompiled functions with `__cdecl16far`: 397
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

Observed direct I/O and hardware assumptions in `viceroy.c`:

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
- `viceroy.c` itself is not yet compiled into the binary; DOS typedef stubs live in
  `src/platform/dos_compat/dos_types.h` for incremental extraction
- Map compositor lookup tables from `VICEROY.EXE` are extracted to `src/data/viceroy_tables.{h,c}`
  (see `docs/viceroy_tables.md`)
- World map view: terrain + partial PHYS0 overlays (coasts, some forests/hills/rivers) —
  good enough for bring-up; see **Deferred: map compositor** below
- Europe screen bring-up: `EUROPE.PIK` + market quotes / dock recruit from `NAMES.TXT`
  (press **E** from the map; see `src/core/europe.c`)

## Deferred: map compositor

Map rendering is intentionally parked. The Linux path in `src/core/map.c` is
fixture-driven and incomplete (missing coast edge bands 112–127 / 132–139,
animation 140–147, generalized forests, roads/resources/fog, accurate river/hill
connectivity).

The real DOS TERRAIN/PHYS0 compositor is reached via **`FUN_281f_*`** (e.g.
`FUN_281f_0e38` from `FUN_1984_010a`). In both `viceroy.c` and the Ghidra listing
`viceroy.asm`, those entry points are **not the compositor** — they are
**RTLink overlay thunks**:

```text
FUN_281f_0e38:
  CALLF  FUN_210d_0dab     ; overlay / "smart vectoring" loader
  JMPF   LAB_0000_0360     ; target inside a loaded overlay page
```

`FUN_210d_0dab` is the RTLink vector manager (`VICEROY.EXE` contains the
`RTLink` / `call to Vector … in Page …` strings). Segment `CODE_99` / `0x281f`
in the listing is mostly those stubs plus undecoded `??` bytes — the page that
holds the real blit/PHYS0 logic was never loaded into the Ghidra database used
for the export. DS tables such as `0x5599` also appear as `??` in the DATA
section of the listing (BSS / runtime-initialized).

**When revisiting map fidelity**, prefer one of:

1. Load VICEROY’s RTLink overlay pages into Ghidra (or dump them after a DOSBox
   run once the map view has been opened), then decompile the real bodies behind
   the `JMPF LAB_0000_*` targets.
2. DOSBox-break on PHYS0 blit with sprite IDs 112–153 and backtrace into the
   loaded overlay.

Until then: keep existing smoke fixtures green; do not expand coast heuristics
unless a user-reported tile is clearly wrong for playability. Related static
tables extracted from the EXE (`0x5599`, `0x54de`, …) still need validation
against recovered overlay code.

