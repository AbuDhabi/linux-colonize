import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;

public class FixFunctionAt extends GhidraScript {
    protected void run() throws Exception {
        String spaceName = getScriptArgs()[0];
        long off = Long.parseLong(getScriptArgs()[1], 16);
        int timeoutSec = getScriptArgs().length > 2 ? Integer.parseInt(getScriptArgs()[2]) : 60;
        AddressSpace sp = currentProgram.getAddressFactory().getAddressSpace(spaceName);
        Address addr = sp.getAddress(off);
        FunctionManager fm = currentProgram.getFunctionManager();
        Function existing = fm.getFunctionAt(addr);
        if (existing == null) {
            existing = createFunction(addr, "FUN_" + spaceName + "__" + Long.toHexString(off) + "_FIXED");
            if (existing == null) { println("createFunction FAILED at " + addr); return; }
        }
        println("Function: " + existing.getName() + " @ " + existing.getEntryPoint()
                + " bodySize=" + existing.getBody().getNumAddresses()
                + " maxAddr=" + existing.getBody().getMaxAddress());
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        long t0 = System.currentTimeMillis();
        DecompileResults r = di.decompileFunction(existing, timeoutSec, monitor);
        long t1 = System.currentTimeMillis();
        println("Decompile took " + (t1 - t0) + " ms");
        if (!r.decompileCompleted()) { println("DECOMPILE FAILED: " + r.getErrorMessage()); return; }
        println("=== DECOMPILED C ===");
        println(r.getDecompiledFunction().getC());
    }
}
