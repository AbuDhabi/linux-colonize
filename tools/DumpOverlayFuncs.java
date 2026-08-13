// Ghidra postScript: dump every function in the OverlayTest project (name,
// address-space, offset within that space, body length) for the address-
// mapping table build. See tools/DumpCanonicalFuncs.java for the other
// half (canonical flattened-EXE project) and docs/rtlink_decode_v2_gap.md.
//
// Read-only: does not modify or save the program.
//
// Usage:
//   analyzeHeadless <projectDir> <projectName> -process <programName> \
//     -postScript DumpOverlayFuncs.java <outCsvPath> \
//     -scriptPath tools -noanalysis -readOnly

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

import java.io.FileWriter;
import java.io.PrintWriter;

public class DumpOverlayFuncs extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: DumpOverlayFuncs.java <outCsvPath>");
            return;
        }
        try (PrintWriter out = new PrintWriter(new FileWriter(args[0]))) {
            out.println("name,space,offset_hex,is_thunk,body_length");
            int n = 0;
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                Address entry = f.getEntryPoint();
                out.println(f.getName() + "," + entry.getAddressSpace().getName() + ","
                        + Long.toHexString(entry.getOffset()) + "," + f.isThunk() + ","
                        + f.getBody().getNumAddresses());
                n++;
            }
            println("Wrote " + n + " functions.");
        }
    }
}
