// Ghidra headless postScript: dump raw memory bytes at space:offset as hex,
// bypassing the Listing/decompiler entirely. Use this to get ground truth
// when a decompile looks implausible (indirect-jump-table misresolution,
// stale disassembly cache, etc) — cross-check against GhidraListInstrs.java's
// (possibly-wrong) instruction rendering of the same range.
// Found real use resolving FUN_1000_8aac's local jump-table dispatcher
// (move_scoring_20e6_full.md "2026-08-15, later pass") after
// GhidraDecompileAt/plain disassembly both mis-chased an indirect jump.
//
// Usage:
//   analyzeHeadless <projectDir> <projectName> -process <programName> \
//     -postScript GhidraDumpBytes.java <space>:<offsetHex> <lenHex> \
//     -scriptPath tools -noanalysis
//
// <space> is "0000" (resident block) or an overlay name like "OVL14_L0000".
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

public class GhidraDumpBytes extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        String[] parts = args[0].split(":");
        String spaceName = parts[0];
        long off = Long.parseLong(parts[1], 16);
        long len = Long.parseLong(args[1], 16);
        Memory memory = currentProgram.getMemory();
        AddressSpace space = null;
        if (spaceName.equals("0000") || spaceName.equalsIgnoreCase("resident")) {
            space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        } else {
            for (MemoryBlock b : memory.getBlocks()) {
                if (b.getName().equals(spaceName)) { space = b.getStart().getAddressSpace(); break; }
            }
        }
        Address start = space.getAddress(off);
        byte[] buf = new byte[(int) len];
        memory.getBytes(start, buf);
        StringBuilder sb = new StringBuilder();
        for (byte b : buf) sb.append(String.format("%02x", b));
        println("===== " + args[0] + " len=" + args[1] + " =====");
        println(sb.toString());
    }
}
