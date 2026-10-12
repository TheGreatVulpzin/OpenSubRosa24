// Decompile functions to a .c file (Ghidra pseudo-C).
//
// Usage (headless, with -process <program> -noanalysis):
//   -postScript DecompileFunctions.java <output.c> [rva ...]
// Without RVAs, it decompiles all functions of the program.
//@category SubVulpes

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class DecompileFunctions extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        String out = args[0];
        long base = currentProgram.getImageBase().getOffset();

        List<Function> funcs = new ArrayList<>();
        if (args.length > 1) {
            for (int i = 1; i < args.length; i++) {
                long rva = Long.decode(args[i]);
                Function f = getFunctionAt(toAddr(base + rva));
                if (f == null) f = getFunctionContaining(toAddr(base + rva));
                if (f != null) funcs.add(f);
                else println("No function at " + args[i]);
            }
        } else {
            currentProgram.getListing().getFunctions(true).forEach(funcs::add);
        }

        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
            for (Function f : funcs) {
                if (monitor.isCancelled()) break;
                if (f.isThunk() || f.isExternal()) continue;
                DecompileResults r = di.decompileFunction(f, 120, monitor);
                w.printf("// rva 0x%x  %s%n", f.getEntryPoint().getOffset() - base, f.getName());
                w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// failed: " + r.getErrorMessage());
                w.println();
            }
        }
        di.dispose();
        println("Decompiled: " + out);
    }
}
