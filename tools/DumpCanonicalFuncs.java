// Ghidra postScript: dump every function in the canonical flattened-EXE
// project (decompiled-colonize / VICEROY_OUT_2.EXE) with its name and its
// byte offset within the imported file, via MemoryBlockSourceInfo. That
// file offset is what ties this addressing scheme back to the original
// VICEROY.EXE segment table (tools/rtlink_overlay_extract.py's manifest)
// and from there to the per-overlay addressing in the OverlayTest project.
// See docs/rtlink_decode_v2_gap.md ("address-mapping table").
//
// Read-only: does not modify or save the program.
//
// Usage:
//   analyzeHeadless <projectDir> <projectName> -process VICEROY_OUT_2.EXE \
//     -postScript DumpCanonicalFuncs.java <outCsvPath> \
//     -scriptPath tools -noanalysis -readOnly

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.mem.MemoryBlockSourceInfo;

import java.io.FileWriter;
import java.io.PrintWriter;
import java.util.List;

public class DumpCanonicalFuncs extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: DumpCanonicalFuncs.java <outCsvPath>");
            return;
        }
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            out.println("name,address,file_offset_hex");
            int written = 0, skipped = 0;
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                Address entry = f.getEntryPoint();
                MemoryBlock block = currentProgram.getMemory().getBlock(entry);
                if (block == null) {
                    skipped++;
                    continue;
                }
                List<MemoryBlockSourceInfo> infos = block.getSourceInfos();
                if (infos.isEmpty()) {
                    skipped++;
                    continue;
                }
                // Use the source info that actually contains this address (a block can
                // rarely have more than one, e.g. if bytes came from multiple imports).
                MemoryBlockSourceInfo match = null;
                for (MemoryBlockSourceInfo si : infos) {
                    if (si.contains(entry)) {
                        match = si;
                        break;
                    }
                }
                if (match == null) {
                    skipped++;
                    continue;
                }
                long fileOffset = match.getFileBytesOffset(entry);
                out.println(f.getName() + "," + entry + "," + Long.toHexString(fileOffset));
                written++;
            }
            println("Wrote " + written + " functions, skipped " + skipped + " (no file-backed source info).");
        }
    }
}
