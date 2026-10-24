# rtlink_decode V2 investigation (archived 2026-10-24)

STATUS: archived 2026-10-24 from docs/rtlink_decode_v2_gap.md — history, not a spec. Patch 1 (applied), the do-not-apply-patch-2 trap, the overlay-addressed Ghidra tooling and the address-mapping method stay live in [rtlink_decode_v2_gap.md](../rtlink_decode_v2_gap.md). Inner headings demoted one level.

## Investigation: V2 relocation gap and the two patches (2026-08-13)

### What I found

Reading `rtlink_decode.cpp` turned up two spots where V2 support is explicitly
incomplete/short-circuited — the author's own comments say so:

1. **`processExecutable()`, per-segment relocation pass** (~line 805, old code):
   ```cpp
   if (rtlinkVersion == VERSION2) {
       // No processing needed
   } else if (selector >= se.loadSegment && selector < (se.loadSegment + se.codeSize / 16)) {
       ...
   } else if (selector >= dataSeg.loadSegment) {
       ...
   }
   ```
   Each RTLink segment declares its **own** relocation list in its on-disk header
   (`variation2.cpp::loadSegmentListV2`, verified populated correctly — matches the
   527/826/296/… counts `rtlink_decode VICEROY.EXE` prints per segment). For V1/V3 those
   embedded far-pointer words get rewritten to match the new flattened file layout; for
   V2 this rewrite is **skipped entirely**, leaving the *original DOS runtime* segment
   value baked into the output file.

2. **`processExecutable()`, original-EXE-relocation pass** (~line 851):
   ```cpp
   if (se.isExecutable && selector >= se.loadSegment
       && (se.isDataSegment || selector < (se.loadSegment + se.codeSize / 16))
       && (se.isDataSegment || rtlinkVersion == VERSION1)) {
   ```
   Comment directly above it: *"for version 2 games, I had issues with spurious
   references getting remapped.. it looks like some rtlink segments load over startup
   code areas. So to be on the safe side, for them I only adjust segment mappings into
   the data segment."* i.e. the author hit false positives and deliberately caged V2 to
   data-segment-only fixups here.

### Verified experimentally (not just read the code)

Rebuilt `rtlink_decode` standalone (ScummVM's `common/` headers are header-only enough
to compile outside the full ScummVM tree — `g++ -DHAVE_CONFIG_H
-DFORBIDDEN_SYMBOL_ALLOW_ALL`, `config.h` just needs `#define SCUMM_LITTLE_ENDIAN 1`).
Confirmed the rebuild is byte-identical to the tool's existing `VICEROY_OUT.EXE`
(md5 `54ecdb82e3d080d59c1a5319218b61ae`) before changing anything.

- **Patch 1** (remove the V2 skip in the per-segment pass): rebuilt, regenerated
  `VICEROY_OUT.EXE`, diffed against the original output. **Exactly 10 words differ**,
  all the same fixup: stale selector `0x1b5a` (the original DOS overlay-area segment,
  == `dataSeg.loadSegment`) corrected to `0x6b29` (the data segment's real position in
  the flattened file, `(dataSeg.outputCodeOffset - outputCodeOffset)/16`). These are
  embedded far pointers **to the shared data segment** sitting inside overlay code
  bodies (segments 2/3/6/9/13/25 by rough offset mapping) that were never being fixed
  up for V2. Small in count, but each one is a seed a decompiler follows — worth having
  correct regardless.
- **Patch 2** (also relax the second, author-caged pass for V2): rebuilt, regenerated.
  **222 words changed**, but they land at file offsets 46401–51354 and 289902 —
  *inside the static/resident portion of the binary* (segment 0's own `codeOffset` is
  `0x1d9a0` = 121760, past `outputCodeOffset` 0xb400 but these hits are lower still,
  before segment 0 even starts in the file). That's exactly the "spurious references /
  startup code area" false-positive the author's comment warned about. **Don't enable
  this one** — reproduced the known failure mode, not a fix.

### Net assessment

Real bug, narrow blast radius: only 10 stale pointers exist game-wide for this binary,
and all 10 are data-segment references, not cross-overlay code targets. That's too small
a number to be the primary driver of 386 Ghidra `WARNING:`s by itself — worth applying
(it's free and correct) but **don't expect it alone to clear the warning backlog**. The
bulk of the 386 is more likely genuine Ghidra 16-bit real-mode limitation (spacebase
tracking, switch/jump-table recovery) hitting overlay bodies, as already noted in
[decomp_inventory.md](../decomp_inventory.md).

## Patch 1 measurement and the original ranked plan (2026-08-13)

### Measured after the patch (Ghidra re-export, `viceroy_unpacked_2.c`/`.asm`)

- `WARNING:` count: **386 → 369** (-17, ~4%).
- Function definitions: 1196 → 1187; 7 `FUN_*` symbols vanished entirely — a
  7-function garbage chain at `FUN_1d1d_1d79`…`FUN_1d1d_1e3f` (all under 200 bytes
  combined) collapsed away, most likely a jump-table region that was getting
  mis-split into phantom functions before.
- Of the 4 named trouble functions above: `FUN_4d56_417e`, `FUN_4d56_2820`, and
  `FUN_521d_5b66` are **byte-for-byte unchanged**, warnings and all (expected —
  none of them sit near one of the 10 patched pointers). `FUN_4d56_4528` lost
  its warnings but now hits `Unable to decompile 'FUN_4d56_4528'` instead of
  emitting garbage C with a warning — safer per this project's own rule
  (confidently-wrong beats nothing), but still not usable C; still needs the
  manual/dynamic route, see `indian_settlement_4528.md`.

Confirms the prediction above: real, worth keeping, does not clear the backlog.
369 warnings remain.

### Recommended path forward, ranked

1. **Done.** Patch 1 applied, `VICEROY_OUT_2.EXE` regenerated, re-run through the
   existing Ghidra import/export pipeline (`viceroy_unpacked_2.c`/`.asm`, produced
   by the user, not by this tooling): 386 → 369 warnings (-17, ~4%), 7 phantom
   functions collapsed away. 3 of the 4 previously-named trouble functions
   (`FUN_4d56_417e`, `FUN_4d56_2820`, `FUN_521d_5b66`) unchanged; `FUN_4d56_4528`
   lost its warnings but now fails to decompile at all rather than emitting wrong
   C. Confirms the prediction below — real, worth keeping, not a backlog-clearer.
2. **If the warning count barely moves (expected, given only 10 words changed):** the
   real lever is dynamic ground-truth, not further static-relink archaeology. The repo
   already has the infrastructure for this (`dosbox-x-dumps/`, `original_memory_dumps/`
   — used today for save-state RE). Extend the same approach to *code*: run the real
   `VICEROY.EXE` under DOSBox-X's debugger, breakpoint on the RTLink overlay-load
   routine, and on each load dump (a) which overlay index loaded and (b) an execution
   trace / code-fetch log for that session. Cross-reference against Ghidra's static
   call graph: any address Ghidra treated as data/unreached that the CPU actually
   fetched as an opcode (or vice versa) is a confirmed desync, and now you have the
   *real* byte stream to re-disassemble it from — instead of guessing from Ghidra's
   already-confused output.

## Overlay extractor: resident/data region correction (2026-08-14)

   **Resident/data region correction:** the first version of this tool found
   the static/resident region by scanning for the `"MS Run-Time"` libc
   signature and importing only that narrow ~11KB tail, based at the DOS
   selector (0x1b5a) where that tail happens to sit. That value is real (it's
   `rtlink_decode`'s own internal `dataSeg.loadSegment`, and matches the 10
   far-pointer fixups documented above) but it's an offset *within* the
   larger static region, not that region's own base — using it as the base
   left ~112KB of actual resident code (including the RTLink thunk-stub
   table itself) unextracted. Symptom: exporting a single overlay to C (e.g.
   `FUN_OVL02_L0000__000070`) showed dozens of `thunk_EXT_FUN_1000_*` /
   `halt_baddata()` functions — every call from the overlay into the
   resident portion landed on undefined memory. Fix: the static region is
   simply everything between the MZ header's `codeOffset` and the first
   overlay's header (~120KB), based at runtime segment **0** (standard DOS
   relocatable-EXE convention — the whole span is compiled assuming CS=0000
   in its own frame). Re-verified against the same function: all
   `thunk_EXT_FUN_1000_*`/`halt_baddata` calls resolved to real, named,
   decompiled functions (`FUN_1000_84f2()`, `FUN_1000_8628(...)`, etc).
   Project-wide: resident space went from 0 functions found to 1,429;
   totals **1,960 functions / 162,921 instructions**, **zero**
   `halt_baddata` thunks anywhere (was 44 in that one function alone).

## Superseded recommendation 4 (implemented by tools/GhidraImportOverlays.java)

4. **For any function actually needed next** (per the existing "check for `WARNING:`
   above the function before porting" rule in decomp_inventory.md): prefer Ghidra's
   native **overlay memory-block** feature over trusting the flattened file's absolute
   addresses. Import each RTLink segment as its own named overlay block at its *original*
   `loadSegment` address (available from `rtlink_decode VICEROY.EXE`'s info-mode segment
   table — no `Output.exe` argument needed) instead of the flattened output offset. Since
   the original small-model in-segment jumps/calls were relative to that same original
   base, this sidesteps relocation math (and its V2 gaps) entirely for anything that
   isn't a genuine cross-overlay far call — those are enumerable from the same tool's
   jump-thunk table (`loadJumpList`, also printed by info mode).
