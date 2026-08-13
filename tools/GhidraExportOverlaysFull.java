// Ghidra headless postScript: export the full analyzed OverlayTest program
// (resident/data block + all 31 RTLink overlay blocks) to ASM + C, with an
// explicit AddressSetView unioning *every* memory block — including all the
// named overlay spaces.
//
// Why this exists: a GUI "File > Export Program" run on this project only
// captured ~29 of the ~531 overlay-space function definitions (resident
// space came through fine). Most likely cause: Ghidra's exporters default
// to the program's "default" address space when no explicit address set is
// given, which doesn't automatically include separately-named overlay
// spaces. Building the AddressSetView ourselves, from every block
// regardless of space, sidesteps that.
//
// Usage:
//   analyzeHeadless <projectDir> <projectName> -process <programName> \
//     -postScript GhidraExportOverlaysFull.java <outBaseName> \
//     -scriptPath tools -noanalysis
//
// Writes <outBaseName>.c and <outBaseName>.asm next to wherever this is run
// from (or pass an absolute path prefix).

import ghidra.app.util.exporter.AsciiExporter;
import ghidra.app.util.exporter.CppExporter;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.app.script.GhidraScript;

import java.io.File;

public class GhidraExportOverlaysFull extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            println("Usage: GhidraExportOverlaysFull.java <outBaseName>");
            return;
        }
        String outBase = args[0];

        Memory memory = currentProgram.getMemory();
        AddressSet all = new AddressSet();
        int blockCount = 0;
        for (MemoryBlock b : memory.getBlocks()) {
            all.add(b.getStart(), b.getEnd());
            blockCount++;
        }
        println("Built full AddressSetView from " + blockCount + " blocks, "
                + all.getNumAddresses() + " total addresses.");

        File cFile = new File(outBase + ".c");
        println("Exporting C to " + cFile.getAbsolutePath() + " ...");
        CppExporter cpp = new CppExporter();
        boolean cOk = cpp.export(cFile, currentProgram, all, monitor);
        println("C export " + (cOk ? "succeeded" : "FAILED"));

        File asmFile = new File(outBase + ".asm");
        println("Exporting ASM to " + asmFile.getAbsolutePath() + " ...");
        AsciiExporter ascii = new AsciiExporter();
        boolean asmOk = ascii.export(asmFile, currentProgram, all, monitor);
        println("ASM export " + (asmOk ? "succeeded" : "FAILED"));
    }
}
