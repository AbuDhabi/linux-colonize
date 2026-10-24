# Decompiled Surface Inventory

For a navigable index of decomp sources, `COLONIZE/` data files, and DOSBox memory
dumps, see [original_index.md](original_index.md). Manual feature coverage vs the
OpenCol port: [manual_gap.md](manual_gap.md). European / Indian AI FUN_* inventory (history):
[archive/port_plan_deferred_ai_track.md](archive/port_plan_deferred_ai_track.md).

This repository keeps Ghidra exports of `VICEROY.EXE` / `MAPEDIT.EXE` under
[`original_sources_decompiled/`](../original_sources_decompiled/) for reverse-engineering
reference. They are not buildable with a modern OpenCol compiler and retain DOS
memory-model / runtime artifacts.

| File | Source | Notes |
|------|--------|-------|
| `original_sources_decompiled/viceroy_unpacked.c` / `.asm` | Unpacked EXE | ~125k / ~305k lines; overlay-resident code is present |
| `original_sources_decompiled/viceroy_unpacked_2.c` / `.asm` | Re-export after [rtlink patch 1](rtlink_decode_v2_gap.md) | 369 warnings vs 386 |
| `original_sources_decompiled/viceroy_overlays.c` / `.asm` | Overlay-addressed project | `FUN_0000_oooo` / `OVLnn_L0000` names; map via `tools/address_mapping.csv` |
| `original_sources_decompiled/*_ndisasm.asm` | raw `ndisasm` | Byte-level reads (dropped far-call args) |
| `original_sources_decompiled/mapedit.c` | `MAPEDIT.EXE` | Static map feature art |

Prefer **`viceroy_unpacked.*`** when chasing map-view or overlay call chains.

Ghidra synthetic symbols: globals `DAT_xxxx_xxxx`, labels `LAB_xxxx_xxxx`,
switch labels `caseD_*`. The `FUN_ssss_oooo` segment prefix is a practical
first-pass clustering (e.g. `15eb`/`1d1d` are high-density logic).

## Known Ghidra Disassembly Faults (check before trusting a function)

`viceroy_unpacked.c` carries **386** Ghidra `WARNING:` comments (`grep -c
"WARNING: Instruction at\|WARNING: Removing unreachable\|WARNING: Unable to
track spacebase"`), each sitting immediately above the function it applies
to. These are Ghidra admitting its own **disassembly** — not just the
higher-level C reconstruction — went wrong near that function: typically an
instruction-boundary desync ("Instruction at (ram,X) overlaps instruction at
(ram,Y)", meaning decode started mid-instruction and re-synced by luck, so
everything downstream is suspect until it does), a stack-tracking failure
("Unable to track spacebase fully for stack" → the messy `unaff_SI/DI/ES/SS`
/ `in_stack_*` locals you'll see flooding the function's C body), or blocks
Ghidra pruned as unreachable (a symptom of the same confusion, not
necessarily truly dead code).

**Before attempting to port or deep-RE any FUN_\* body, check the ~5 lines of
C source right above its declaration for one of these comments.** If present,
treat the function as **not safely portable from the existing decomp output
as-is** — porting from a corrupted disassembly risks *confidently wrong*
first-draft logic, which is worse than leaving it thin/stub. Fixing it for
real needs manual re-disassembly from the correct byte offset, which is
out-of-band from reading Ghidra's existing C/ASM output (that output is the
thing that's wrong).

Every function flagged during the 2026-08-13 push was re-disassembled clean via the
overlay project ([rtlink_decode_v2_gap.md](rtlink_decode_v2_gap.md)); the per-function
tables, the catalog-tail sweep and the resident pocket are in
[archive/decomp_inventory_history_2026-10-24.md](archive/decomp_inventory_history_2026-10-24.md).
Traps that still apply:

- `FUN_521d_20e6`: the canonical export's body drifted from the fixed recovery;
  [`move_scoring_20e6_full.md`](../original_sources_annotated/ai/move_scoring_20e6_full.md)
  is authoritative for this function.
- `FUN_521d_5b66` is a 198-byte dispatcher; the ~1815-line body once attributed to it
  was desync content from the next RTLink segment (`euro_unit_act.md` correction note).
- `FUN_15eb_1d4c`: jump table and `.asm` are clean; only the `.c` exports
  (`viceroy_unpacked_2.c`, `viceroy_overlays.c`) synthesize wrong case bodies. Read the
  `.asm` ([`manufacturing_worker_calc_1d4c.md`](../original_sources_annotated/turn/manufacturing_worker_calc_1d4c.md)).
- `FUN_0000_fe5e`: a real pcode error (flow into `1000:ff05`, 0x1c95 past the resident
  `codeSize` 0x1e270), probably a computed far-call target. Do not "widen the boundary".
- `FUN_5fef_0000`, `OVL12_L0000:0`, `FUN_684c_08c0`: decompiler bugs (`CALLF 0x1000:XXXX`
  resolution, `Unable to resolve constructor`) over clean disassembly.
- A `WARNING:` or pcode error attributed to a function through *another* function's
  inlining is not evidence; decompile the callee directly first (5 of the 6
  resident-pocket "corruptions" were this).

The warning-corruption lead is **exhausted** (2026-08-13 sweep of all ~159
warning-adjacent names): nothing left carries the severe signature (`Instruction ...
overlaps`, `bad instruction data`, decompile timeout); the rest is cosmetic decompiler
noise (`Removing unreachable block`, spacebase, jumptable limits). Only a new starting
signal reopens it.

**Flattening step** (`rtlink_decode` V2): [rtlink_decode_v2_gap.md](rtlink_decode_v2_gap.md).
A real V2 relocation gap (10 stale data-segment far pointers) is fixed by patch 1,
applied in `viceroy_unpacked_2.*` (386 → 369 warnings); it is not what drives the warnings.

## Method: asking the user to identify things is a legitimate RE tool

When a mechanic, string, sprite or sound will not resolve from the decompile, `GAME.TXT`
or `.asm`, describe precisely what the code does and what is uncertain, then ask the user
(he has played the game) or propose a concrete DOSBox-X capture plan (function/address to
watch, what to look for). His testimony is evidence on par with a capture
(docs/conventions.md §4 tier 1). Worked examples (Incite Indians `417e`, `@INDIANBEGFOOD`
sign convention, the popup-id `BPM` capture):
[archive/decomp_inventory_history_2026-10-24.md](archive/decomp_inventory_history_2026-10-24.md).

## DOS/hardware-coupled surfaces

Direct hardware I/O in the exports (VGA DAC `out 0x3c8`/`0x3c9`, retrace `in 0x3da`,
PIT `out 0x43,0x36` / `out 0x40`, VRAM segment `0xa000`, BIOS tick `DAT_0000_046c`)
maps to the SDL2 platform layer, never to raw port I/O
([architecture.md](architecture.md) "Layering").

## Moved elsewhere

- Bring-up / feature status: [manual_gap.md](manual_gap.md).
- End-of-turn pipeline: [turn_between_players.md](turn_between_players.md) (starting
  gold by difficulty: [difficulty.md](difficulty.md)).
- NEW WORLD map generation and the MAPEDIT static compositor: [assets.md](assets.md)
  "Map overlay compositing" and "Map generation (NEW WORLD)".
