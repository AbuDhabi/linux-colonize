// Ghidra headless postScript: build a throwaway program that holds every
// RTLink v2 overlay segment of VICEROY.EXE at its own true DOS load
// address, each in its own named Ghidra *overlay* address space, instead
// of trusting rtlink_decode's single flattened relink — see
// docs/rtlink_decode_v2_gap.md for why that flattening is imperfect for V2.
//
// Run against a program already imported as raw binary (BinaryLoader,
// default base 0) from seg_data_resident.bin — this script relocates that
// base block to its real address and adds one overlay block per overlay
// segment, loading bytes straight from the .bin files
// tools/rtlink_overlay_extract.py already produced.
//
// Usage (from repo root, after running rtlink_overlay_extract.py):
//   analyzeHeadless <projectDir> <projectName> \
//     -import /path/to/overlays_extracted/seg_data_resident.bin \
//     -processor "x86:LE:16:Real Mode" -cspec default \
//     -postScript GhidraImportOverlays.java <extractedDir> [residentLoadSegmentHex] \
//     -scriptPath tools -noanalysis
//
// <extractedDir> is the directory rtlink_overlay_extract.py wrote
// segments.tsv + seg_*.bin into. residentLoadSegmentHex defaults to 0 — the
// static/resident region (now the *whole* span from the MZ header's
// codeOffset to the first overlay's header, not just the narrow
// "MS Run-Time"-bounded tail) is compiled assuming CS=0000 in its own
// frame, standard DOS relocatable-EXE convention. Override only if pointed
// at a different RTLink v2 title with different addressing.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSpace;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

import java.io.File;
import java.io.FileInputStream;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

public class GhidraImportOverlays extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: GhidraImportOverlays.java <extractedDir> [residentLoadSegmentHex]");
            return;
        }
        String extractedDir = args[0];
        int residentLoadSegment = args.length > 1 ? Integer.parseInt(args[1], 16) : 0x0000;

        Memory memory = currentProgram.getMemory();
        AddressSpace space = currentProgram.getAddressFactory().getDefaultAddressSpace();

        // Step 1: relocate the resident/data block (imported at default base 0)
        // to its real DOS load segment.
        MemoryBlock[] blocks = memory.getBlocks();
        MemoryBlock residentBlock = null;
        if (blocks.length != 1) {
            println("WARNING: expected exactly one block from the initial raw import, found "
                    + blocks.length + " — skipping the move, check manually.");
        } else {
            residentBlock = blocks[0];
            Address newBase = space.getAddress(((long) residentLoadSegment) * 16L);
            if (!residentBlock.getStart().equals(newBase)) {
                println("Moving resident/data block " + residentBlock.getStart() + " -> "
                        + newBase + " (loadSegment " + Integer.toHexString(residentLoadSegment) + "h)");
                memory.moveBlock(residentBlock, newBase, monitor);
                residentBlock.setName("RESIDENT_DATA");
                // moveBlock can hand back a distinct block instance — refetch by name
                // to be safe rather than trust the pre-move reference stays valid.
                residentBlock = memory.getBlock("RESIDENT_DATA");
            } else {
                println("Resident/data block already at " + newBase);
            }
        }

        // Step 2: add one overlay block per RTLink overlay segment.
        // segments.tsv columns: segmentIndex  isDataSegment(0/1)  loadSegmentHex  codeSize  file
        List<String> lines = Files.readAllLines(Paths.get(extractedDir, "segments.tsv"));
        int created = 0;
        List<MemoryBlock> overlayBlocks = new ArrayList<>();
        for (String line : lines) {
            if (line.trim().isEmpty()) {
                continue;
            }
            String[] f = line.split("\t");
            int segmentIndex = Integer.parseInt(f[0]);
            boolean isData = f[1].equals("1");
            if (isData) {
                continue; // that's the block we just moved into place, not an overlay
            }
            int loadSegment = Integer.parseInt(f[2], 16);
            long codeSize = Long.parseLong(f[3]);
            String fileName = f[4];

            String name = String.format("OVL%02d_L%04x", segmentIndex, loadSegment);
            Address start = space.getAddress(((long) loadSegment) * 16L);
            File binFile = new File(extractedDir, fileName);

            println("Creating overlay '" + name + "' at " + start + ", " + codeSize
                    + " bytes, from " + fileName);

            MemoryBlock ovlBlock;
            try (FileInputStream fis = new FileInputStream(binFile)) {
                ovlBlock = memory.createInitializedBlock(name, start, fis, codeSize, monitor, true);
            }
            overlayBlocks.add(ovlBlock);
            created++;
        }

        println("Done: 1 resident/data block + " + created + " overlay blocks.");
        println("Each overlay's internal near/far refs resolve correctly with zero relocation "
                + "math needed (see docs/rtlink_decode_v2_gap.md) — only genuine cross-overlay "
                + "far calls (the RTLink thunk table) need manual cross-referencing between spaces.");

        // Raw import + overlay blocks have no entry point / symbols, so nothing seeds
        // analysis on its own — a plain analyzeAll() here finds zero instructions. Sweep
        // every block start-to-end with linear-descent disassembly first (best effort;
        // this is a raw code blob, not everything in it may actually be code — bytes that
        // don't decode as valid instructions are just left undefined, harmless), *then*
        // run analyzeAll() so the function-boundary / stack / decompiler-markup analyzers
        // have something to work with.
        println("Sweep-disassembling resident block + " + overlayBlocks.size() + " overlay blocks...");
        if (residentBlock != null) {
            sweepDisassemble(residentBlock);
        }
        for (MemoryBlock b : overlayBlocks) {
            sweepDisassemble(b);
        }

        println("Running full auto-analysis over all blocks — this is the slow part, be patient...");
        analyzeAll(currentProgram);
        println("Analysis complete.");
        // No explicit save() here — analyzeHeadless auto-commits the program after the
        // postScript returns; calling save() ourselves hits ReadOnlyException (the program
        // handle is opened read-only from the script's point of view during import).
    }

    /**
     * Linear-descent disassembly sweep of an entire block: try disassemble() at every
     * address not already covered by a code unit. disassemble() itself follows control
     * flow from each start point, so this only actually pays the "try" cost at gaps
     * between flow-reachable runs (typically: data embedded between functions, or a
     * function only reachable via a cross-overlay far call we have no seed for).
     */
    private static boolean isRealCodeUnit(CodeUnit cu) {
        // Every initialized byte starts with an implicit 1-byte "undefined data" code
        // unit — that's not something to skip over, it's exactly what still needs
        // disassembling. Only Instructions and *defined* Data mean "already covered".
        if (cu == null) {
            return false;
        }
        if (cu instanceof Data) {
            return ((Data) cu).isDefined();
        }
        return true; // Instruction
    }

    private void sweepDisassemble(MemoryBlock block) throws Exception {
        Listing listing = currentProgram.getListing();
        Address addr = block.getStart();
        Address end = block.getEnd();
        while (addr != null && addr.compareTo(end) <= 0) {
            if (monitor.isCancelled()) {
                return;
            }
            CodeUnit cu = listing.getCodeUnitContaining(addr);
            if (isRealCodeUnit(cu)) {
                addr = cu.getMaxAddress().add(1);
                continue;
            }
            disassemble(addr); // best-effort; leaves addr undefined if it's not valid code
            CodeUnit after = listing.getCodeUnitContaining(addr);
            addr = isRealCodeUnit(after) ? after.getMaxAddress().add(1) : addr.add(1);
        }
    }
}
