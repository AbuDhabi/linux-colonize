// Ad hoc: list raw disassembled instructions in one or more [space:start,
// space:start+len) ranges, in a single headless launch (batches multiple
// targets to avoid repeated JVM startup cost).
// Usage: analyzeHeadless <proj> <name> -process <program> -postScript
//   GhidraListInstrs.java <space1>:<offsetHex1> <lenHex1> [<space2>:<offsetHex2> <lenHex2> ...]
//   -scriptPath tools -noanalysis
// Deliberately does NOT call createFunction — plain disassemble() only, to
// avoid the reachability-walk jump-table misresolution bug this tool was
// built to route around (move_scoring_20e6_full.md "2026-08-15, later pass").
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.mem.Memory;

public class GhidraListInstrs extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 2 || args.length % 2 != 0) {
            println("Usage: GhidraListInstrs.java <space>:<offsetHex> <lenHex> [...]");
            return;
        }
        Memory memory = currentProgram.getMemory();
        for (int pair = 0; pair < args.length; pair += 2) {
            String[] parts = args[pair].split(":");
            String spaceName = parts[0];
            long off = Long.parseLong(parts[1], 16);
            long len = Long.parseLong(args[pair + 1], 16);

            AddressSpace space = null;
            if (spaceName.equals("0000") || spaceName.equalsIgnoreCase("resident")) {
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
                println("ERROR: no space " + spaceName);
                continue;
            }
            Address start = space.getAddress(off);
            Address end = space.getAddress(off + len);
            println("===== " + args[pair] + " (" + start + ") len=" + args[pair + 1] + " =====");
            disassemble(start);
            Listing listing = currentProgram.getListing();
            Instruction instr = listing.getInstructionAt(start);
            if (instr == null) {
                instr = listing.getInstructionContaining(start);
            }
            while (instr != null && instr.getAddress().compareTo(end) < 0) {
                StringBuilder hx = new StringBuilder();
                for (byte b : instr.getBytes()) hx.append(String.format("%02x", b));
                println(instr.getAddress() + "  " + instr.toString() + "   ; instrBytes=" + hx.toString());
                instr = instr.getNext();
            }
        }
    }
}
