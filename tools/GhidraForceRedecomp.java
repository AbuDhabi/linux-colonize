// Ghidra headless postScript: clear existing Listing analysis over
// [addr, addr+len), then disassemble + createFunction + decompile fresh.
// Use to confirm/reproduce a decompiler jump/call-target misresolution bug
// (decomp_inventory.md's "684c_08c0"/"15eb_1d4c" class): if the freshly
// rebuilt function still grows past a plain linear ndisasm's real
// boundary, the bug is real and reproducible, not a stale-cache artifact.
// Cite: move_scoring_20e6_full.md "2026-08-15, later pass" (FUN_0000_4fa8).
//
// Usage:
//   analyzeHeadless <projectDir> <projectName> -process <programName> \
//     -postScript GhidraForceRedecomp.java <space>:<offsetHex> [lenHex] \
//     -scriptPath tools -noanalysis
//
// lenHex bounds the clear range (default 0x300); does not bound the
// resulting function, which may still grow past it via createFunction's
// own reachability walk — that growth is exactly the signal being tested.
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.util.task.ConsoleTaskMonitor;

public class GhidraForceRedecomp extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        String[] parts = args[0].split(":");
        String spaceName = parts[0];
        long off = Long.parseLong(parts[1], 16);
        long len = args.length > 1 ? Long.parseLong(args[1], 16) : 0x300;

        AddressSpace space = null;
        if (spaceName.equals("0000") || spaceName.equalsIgnoreCase("resident")) {
            space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        } else {
            for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
                if (b.getName().equals(spaceName)) { space = b.getStart().getAddressSpace(); break; }
            }
        }
        Address start = space.getAddress(off);
        Address end = space.getAddress(off + len);
        println("===== " + args[0] + " (" + start + ") =====");

        currentProgram.getListing().clearCodeUnits(start, end, false);
        currentProgram.getFunctionManager().getFunctionContaining(start);

        boolean ok = disassemble(start);
        println("disassemble: " + ok);
        Function f = createFunction(start, null);
        if (f == null) {
            println("ERROR: createFunction failed");
            return;
        }
        println("Created: " + f.getName() + " @ " + f.getEntryPoint() + " end=" + f.getBody().getMaxAddress());

        DecompInterface decomp = new DecompInterface();
        decomp.openProgram(currentProgram);
        decomp.setOptions(new ghidra.app.decompiler.DecompileOptions());
        DecompileResults res = decomp.decompileFunction(f, 60, new ConsoleTaskMonitor());
        if (res == null || !res.decompileCompleted()) {
            println("DECOMPILE FAILED: " + (res != null ? res.getErrorMessage() : "null"));
            return;
        }
        println(res.getDecompiledFunction().getC());
        decomp.dispose();
    }
}
