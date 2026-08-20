// Ghidra headless postScript: clear then disassemble starting EXACTLY at the
// given address, printing the resulting instruction stream. Unlike
// GhidraListInstrs.java, this does NOT fall back to
// Listing.getInstructionContaining(start) when the target address isn't
// already an instruction start from a prior analysis pass — that silent
// fallback is what let move_scoring_20e6_full.md's 2026-08-20 case-4/case-6
// notes cite the wrong bytes (the fallback returned a *different*, nearby,
// already-analyzed instruction 1-2 bytes off from the literal jump-table
// target, and that mismatch went unnoticed). Use this whenever the exact
// byte alignment at a computed/indirect jump target matters — e.g. a local
// jump-table dispatch (FUN_0000_4fa8's `JMP CS:[BX+0xd78]` family) where
// nothing else in the binary's normal control flow happens to enter at that
// same address, so Ghidra's own analyzeAll() sweep never created a genuine
// instruction boundary there.
//
// Usage:
//   analyzeHeadless <projectDir> <projectName> -process <programName> \
//     -postScript GhidraDisasmExact.java <space>:<offsetHex> <lenHex> \
//     -scriptPath tools -noanalysis
//
// <space> is "0000" (resident block) or an overlay name like "OVL14_L0000".
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.mem.Memory;

public class GhidraDisasmExact extends GhidraScript {
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
        Address end = space.getAddress(off + len);
        currentProgram.getListing().clearCodeUnits(start, end, false);
        disassemble(start);
        Listing listing = currentProgram.getListing();
        Instruction instr = listing.getInstructionAt(start);
        while (instr != null && instr.getAddress().compareTo(end) < 0) {
            StringBuilder hx = new StringBuilder();
            for (byte b : instr.getBytes()) hx.append(String.format("%02x", b));
            println(instr.getAddress() + "  " + instr.toString() + "   ; instrBytes=" + hx.toString());
            instr = instr.getNext();
        }
    }
}
