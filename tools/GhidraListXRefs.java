// Ghidra headless postScript: list every reference (call/jump/data read)
// INTO a given address, using Ghidra's own cross-reference index (built
// during analyzeAll(), not a text grep) — the "XREF search this project
// doesn't have quick tooling for" flagged in euro_diplo_153e_full.md's
// T1.11 write-trigger chase (FUN_0000_5b62 callers).
//
// Usage:
//   analyzeHeadless <projectDir> <projectName> -process <programName> \
//     -postScript GhidraListXRefs.java <space>:<offsetHex> \
//     -scriptPath tools -noanalysis
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.ReferenceManager;

public class GhidraListXRefs extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        String[] parts = args[0].split(":");
        String spaceName = parts[0];
        long off = Long.parseLong(parts[1], 16);
        Memory memory = currentProgram.getMemory();
        AddressSpace space = null;
        if (spaceName.equals("0000") || spaceName.equalsIgnoreCase("resident")) {
            space = currentProgram.getAddressFactory().getDefaultAddressSpace();
        } else {
            for (MemoryBlock b : memory.getBlocks()) {
                if (b.getName().equals(spaceName)) { space = b.getStart().getAddressSpace(); break; }
            }
        }
        Address target = space.getAddress(off);
        ReferenceManager rm = currentProgram.getReferenceManager();
        ReferenceIterator it = rm.getReferencesTo(target);
        int count = 0;
        while (it.hasNext()) {
            Reference r = it.next();
            println("XREF  from=" + r.getFromAddress() + "  type=" + r.getReferenceType()
                + "  primary=" + r.isPrimary());
            count++;
        }
        println("TOTAL=" + count);
    }
}
