# `rtlink_decode` V2 relocation gap — root-cause dig for the disassembly quality problem

Context: `VICEROY.EXE` is RTLink/Plus **Version 2** (confirmed by `rtlink_decode`'s own
detector — `Version 2 of RTLink detected`, 21 overlay ("dynamic") segments + 1 static/data
segment, 2260 relocation entries in the MZ header). We flatten it with the ScummVM tool
[`rtlink_decode`](https://github.com/scummvm/scummvm/tree/master/devtools/rtlink_decode)
(local copy: `~/Downloads/tools-rtlink_decode/`) before feeding it to Ghidra, producing
`original_sources_decompiled/viceroy_unpacked.c` (386 Ghidra `WARNING:` comments — see
[decomp_inventory.md](decomp_inventory.md)).

## Result

`rtlink_decode.cpp` `processExecutable()` skips the per-segment relocation pass for V2
(`if (rtlinkVersion == VERSION2) { // No processing needed }`), leaving original DOS
segment values baked into the output; segment bytes are otherwise copied 1:1 to new file
positions. **Patch 1** (below) removes the skip: exactly 10 words change, all stale
selector `0x1b5a` (= `dataSeg.loadSegment`) → `0x6b29`, far pointers to the shared data
segment inside overlay bodies. Applied; the Ghidra re-export `viceroy_unpacked_2.*` went
386 → 369 warnings and lost a 7-function phantom chain (`FUN_1d1d_1d79`…`1e3f`). Real
but small: it is not what drives the warnings. **Patch 2** (letting the second,
author-caged original-EXE-relocation pass run for V2) changes 222 words inside
resident/startup code (file offsets 46401–51354 and 289902), the false-positive failure
the author's comment warns about: never apply it. Investigation narrative and measurements:
[archive/rtlink_decode_v2_investigation_2026-10-24.md](archive/rtlink_decode_v2_investigation_2026-10-24.md).

## Reproduction

```
cd ~/Downloads/tools-rtlink_decode/rtlink_decode
g++ -std=c++11 -DHAVE_CONFIG_H -DFORBIDDEN_SYMBOL_ALLOW_ALL -I. -I../scummvm-master -c rtlink_decode.cpp variation1.cpp variation2.cpp variation3.cpp
g++ rtlink_decode.o variation1.o variation2.o variation3.o -o rtlink_decode_fixed
# apply Patch 1 (see diff below) to rtlink_decode.cpp first, then rebuild
./rtlink_decode_fixed /path/to/VICEROY.EXE VICEROY_OUT_fixed.EXE
```

Patch 1 diff (safe, apply):

```diff
--- a/rtlink_decode.cpp
+++ b/rtlink_decode.cpp
@@ -802,9 +802,7 @@
 			fOut.seek(fileOffset);
 			uint selector = fOut.readWord();

-			if (rtlinkVersion == VERSION2) {
-				// No processing needed
-			} else if (selector >= se.loadSegment && selector < (se.loadSegment + se.codeSize / 16)) {
+			if (selector >= se.loadSegment && selector < (se.loadSegment + se.codeSize / 16)) {
 				int selectorDiff = selector - se.loadSegment;
 				int newSelector = (se.outputCodeOffset - outputCodeOffset) / 16 + selectorDiff;
```

Do **not** apply the second relaxation (extending the `rtlinkVersion == VERSION1` guard
in the later pass to include `VERSION2`) — verified above to corrupt resident/startup
code, not fix anything.

## Overlay-addressed Ghidra project

Prefer this over the flattened file's absolute addresses for any function that still
carries a `WARNING:`: each RTLink segment becomes its own Ghidra overlay block at its
original `loadSegment` address, so in-segment code needs no relocation math; only genuine
cross-overlay far calls (the RTLink jump-thunk table) cross spaces.

1. **Overlay extract + import, tested end-to-end** (`tools/rtlink_overlay_extract.py`
   + `tools/GhidraImportOverlays.java`):

   ```
   python3 tools/rtlink_overlay_extract.py COLONIZE/VICEROY.EXE /path/to/extracted

   /path/to/ghidra/support/analyzeHeadless /path/to/scratch/project ProjectName \
     -import /path/to/extracted/seg_data_resident.bin \
     -processor "x86:LE:16:Real Mode" -cspec default \
     -postScript GhidraImportOverlays.java /path/to/extracted \
     -scriptPath tools -noanalysis
   ```

   `rtlink_overlay_extract.py` is a self-contained reimplementation of
   `rtlink_decode`'s V2 segment-list parser (no dependency on that external
   tool) — it writes each RTLink segment's raw bytes to its own `.bin`, plus
   `segments.tsv`/`segments.json` manifests. Cross-validated against
   `rtlink_decode VICEROY.EXE`'s info-mode listing — codeOffset/codeSize/
   relocation-count match exactly for all 31 overlay segments.

   `GhidraImportOverlays.java` (a Java `GhidraScript`, not Jython/PyGhidra —
   Ghidra 12 dropped Jython and PyGhidra needs a separate Python env this box
   doesn't have set up) bases the resident/data block at runtime segment 0
   (optional `residentLoadSegmentHex` argument, default 0) and adds one Ghidra **overlay** memory
   block per RTLink segment, each at its true `loadSegment<<4` address, bytes
   loaded straight from the extracted `.bin` — zero relocation math. Verified
   by actually running it: produces a Ghidra project with exactly 1 resident
   block + 31 overlay blocks, right sizes/addresses, confirmed by reopening
   the saved project and listing blocks back out.

   This is a **separate, throwaway project** — doesn't touch or rename
   anything in the canonical `viceroy_unpacked*.c` / `FUNCTION_CATALOG.md`
   naming. Use it to get a function's *correct* disassembly (open the right
   `OVLnn_Lxxxx` overlay space in the Ghidra GUI), then port the finding back
   under the function's existing `FUN_` name from the canonical export.

   The script also sweep-disassembles every block (linear-descent: try
   `disassemble()` at every address not already covered by a real code unit —
   raw import has no entry point/symbols to seed analysis from otherwise) and
   then runs full `analyzeAll()`.

   **Resident/data region:** everything between the MZ header's `codeOffset` and the
   first overlay's header (~120KB), based at runtime segment **0**. `0x1b5a` (patch 1's
   selector) is the data segment's position inside that region, not its base; basing the
   block there left the thunk table unextracted (history in the archive file above).

   **Still present, correctly not "fixed"**: implicit-DS near-data refs like
   `*(int *)0x5392` (paired with an `unaff_DS` decompiler local) are
   unchanged — Ghidra can't statically resolve the DS register's runtime
   value there. Same pre-existing limitation already documented for
   `viceroy_unpacked.c`'s own `unaff_*` flood; not a memory-layout problem,
   not something this tool fixes.

   A handful of expected `pcode error at ...: Could not follow disassembly
   flow into non-existing memory` decompiler warnings remain where a
   jump/switch target lands in a *different* overlay space — that's the
   genuine cross-overlay boundary the thunk table covers, not a bug here.

2. **`tools/GhidraDecompileAt.java`** (added 2026-08-14): a third headless
   postScript for the common case of needing just one or two specific
   functions from an already-imported project, when the auto-analysis
   function-start sweep didn't happen to create a boundary at the exact
   target address (common — `address_mapping.csv` calls these "gap" or
   "before-first-function" matches). Usage:
   ```
   analyzeHeadless <projectDir> <projectName> -process <programName> \
     -postScript GhidraDecompileAt.java <space1>:<offsetHex1> [<space2>:<offsetHex2> ...] \
     -scriptPath tools -noanalysis
   ```
   `<space>` is `0000` for the resident block or an overlay name like
   `OVL14_L0000`; `<offsetHex>` has no `0x` prefix. Force-creates a function
   at each address if none already covers it, then decompiles and prints
   its C. Cheaper than a full-project `GhidraExportOverlaysFull.java` export
   when you already know exactly which `FUN_<seg>_<off>` you need (look up
   its overlay/offset in `address_mapping.csv` first).

## Address-mapping table: canonical `FUN_<seg>_<off>` ↔ overlay addressing

`tools/address_mapping.csv` — a row per function in the canonical flattened project
(`decompiled-colonize` / `VICEROY_OUT_2.EXE`, the one behind `viceroy_unpacked*.c`),
giving its corresponding address in the `OverlayTest` project (`viceroy_overlays.c`),
and whether that address lines up with a function boundary there. Turns "is this
WARNING-flagged function corrupted, and what does it really look like" from manual
segment arithmetic into a lookup.

**How it's built** (three pieces tied together by `tools/build_address_mapping.py`):

1. `tools/viceroy_v2_output_layout.json` — per-segment file-offset layout of the
   flattened `VICEROY_OUT_2.EXE` (original `codeOffset`/`codeSize`/`loadSegment` from
   `rtlink_decode`'s own bookkeeping, plus where each segment landed in the flattened
   output file). Captured once via a debug build of `rtlink_decode` printing its
   internal `segmentList` after layout (build recipe under Reproduction above);
   static data specific to `VICEROY.EXE`, not re-derived at build time.
2. `tools/DumpCanonicalFuncs.java` — every function in the canonical project, with its
   byte offset in `VICEROY_OUT_2.EXE` via Ghidra's own `MemoryBlockSourceInfo` (no
   guessing at Ghidra's addressing scheme — it turned out **not** to be the simple
   flat-from-zero convention used for our own resident-block import: the canonical
   project's segment prefixes like `4d56`/`521d`/`1d1d` are real per-module segments
   Ghidra recovered via the file's actual relocation table, applied by its stock MZ-EXE
   loader — reading them back out programmatically sidesteps needing to replicate that).
3. `tools/DumpOverlayFuncs.java` — every function in `OverlayTest`, address + body size.

Given a canonical function's file offset, `build_address_mapping.py` locates which
original segment (or the resident region) it falls in, computes the corresponding
`OverlayTest` address, and looks for a function starting there.

**Results, run against the current state of both projects:** 2,812 canonical functions
mapped. 1,754 land exactly on an `OverlayTest` function start, 9 land inside one
(`contained`); 1,049 don't (`gap` / `before-first-function` / `no-such-space` /
`unmapped-region`) — meaning `OverlayTest`'s analysis didn't happen to create a function
boundary at that exact address, not that the address is wrong. **The address itself is
always given regardless of match** — even an unmatched row tells you exactly where to
look (`OVL13_L0000::4528`, say) in `viceroy_overlays.asm` or the `OverlayTest` GUI.

Spot check on the four originally-named trouble functions (the 2026-08-13 Ghidra-fault
set, [archive/decomp_inventory_history_2026-10-24.md](archive/decomp_inventory_history_2026-10-24.md)): all four now have a precise overlay-side address —
`FUN_4d56_2820`/`FUN_4d56_417e`/`FUN_4d56_4528` → `OVL13_L0000` offsets `2820`/`417e`/
`4528`; `FUN_521d_5b66` → `OVL14_L0000` offset `5b66`. None landed on a clean function
boundary there (`before-first-function` / `gap`) — itself a signal worth noting: the
*canonical* disassembly's function-start point for these may not be where the code
actually begins, consistent with everything else this investigation has found about
them.

**Regenerate** (needs both Ghidra projects unlocked — check for `.lock` files first):

```
analyzeHeadless ~/projects decompiled-colonize -process VICEROY_OUT_2.EXE \
  -readOnly -noanalysis -postScript DumpCanonicalFuncs.java canonical_funcs.csv \
  -scriptPath tools

analyzeHeadless ~/projects/ghidra_overlay_scratch OverlayTest -process seg_data_resident.bin \
  -readOnly -noanalysis -postScript DumpOverlayFuncs.java overlay_funcs.csv \
  -scriptPath tools

python3 tools/build_address_mapping.py tools/viceroy_v2_output_layout.json \
  canonical_funcs.csv overlay_funcs.csv tools/address_mapping.csv
```
