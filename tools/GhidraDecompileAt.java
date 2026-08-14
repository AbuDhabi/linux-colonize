// Ghidra headless postScript: force-create a function at an explicit
// address (space:offset) if none already covers it, then decompile and
// print its C — for targets the auto-analysis function-start search missed
// (common in the OverlayTest project: a real DOS far-function entry that
// lands mid-way through a preceding function's swept-but-unbounded region).
//
// Usage:
//   analyzeHeadless <projectDir> <projectName> -process <programName> \
//     -postScript GhidraDecompileAt.java <space1>:<offsetHex1> [<space2>:<offsetHex2> ...] \
//     -scriptPath tools -noanalysis
//
// <space> is either "0000" (resident block) or an overlay name like
// "OVL14_L0000" (as created by GhidraImportOverlays.java). <offsetHex> is
// the address within that space, no "0x" prefix (e.g. "20e6", "35e").
//
// Prints "===== <space>:<offset> =====" then the decompiled C for each
// target in turn, to stdout (captured in the headless log).

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.util.task.ConsoleTaskMonitor;

public class GhidraDecompileAt extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: GhidraDecompileAt.java <space>:<offsetHex> [...]");
            return;
        }

        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        decomp.setOptions(new ghidra.app.decompiler.DecompileOptions());

        Memory memory = currentProgram.getMemory();
        FunctionManager fm = currentProgram.getFunctionManager();

        for (String arg : args) {
            int colon = arg.indexOf(':');
            if (colon < 0) {
                println("Skipping malformed target: " + arg);
                continue;
            }
            String spaceName = arg.substring(0, colon);
            String offsetHex = arg.substring(colon + 1);

            AddressSpace space = null;
            if (spaceName.equals("0000") || spaceName.equalsIgnoreCase("resident")) {
                // Resident block lives in the default space at its relocated base.
                space = currentProgram.getAddressFactory().getDefaultAddressSpace();
            } else {
                for (MemoryBlock b : memory.getBlocks()) {
                    if (b.getName().equals(spaceName)) {
                        space = b.getStart().getAddressSpace();
                        break;
                    }
                }
            }
            if (space == null) {
                println("===== " + arg + " =====");
                println("ERROR: no memory block/space named '" + spaceName + "'");
                continue;
            }

            long off = Long.parseLong(offsetHex, 16);
            Address entry = space.getAddress(off);

            println("===== " + arg + " (" + entry + ") =====");

            Function f = fm.getFunctionContaining(entry);
            if (f == null || !f.getEntryPoint().equals(entry)) {
                boolean ok = disassemble(entry);
                println("disassemble() at entry: " + ok);
                f = createFunction(entry, null);
                if (f == null) {
                    println("ERROR: createFunction failed at " + entry);
                    continue;
                }
                println("Created function: " + f.getName() + " @ " + f.getEntryPoint());
            } else {
                println("Existing function: " + f.getName() + " @ " + f.getEntryPoint());
            }

            DecompileResults res = decomp.decompileFunction(f, 60, new ConsoleTaskMonitor());
            if (res == null || !res.decompileCompleted()) {
                println("DECOMPILE FAILED: " + (res != null ? res.getErrorMessage() : "null result"));
                continue;
            }
            println(res.getDecompiledFunction().getC());
        }

        decomp.dispose();
    }
}
