// Store the parameters that the decompiler finds for every function that has none stored.
//
// The 024d reuses stack slots for arguments: a caller pushes the arguments once and calls
// two functions with them. When the callee prototype is not stored, the decompiler of the
// caller sees no pushes before the second call and prints "FUN_0040c120();" for a function
// with 6 parameters. The lifted C does not compile (C2198, too few arguments). With the
// parameters stored, the decompiler reads the arguments from the stack slots.
//
// Only the parameters are stored. The return type and the calling convention stay as they
// are (FixReturnTypes.java and FixRegisterPreservation.java set them).
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript CommitParams.java
//@category SubVulpes

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunctionDBUtil;
import ghidra.program.model.symbol.SourceType;

public class CommitParams extends GhidraScript {

    @Override
    protected void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        int stored = 0, skipped = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (monitor.isCancelled()) break;
            if (f.isThunk() || f.isExternal() || f.getParameterCount() > 0 || f.hasCustomVariableStorage()
                    || f.hasVarArgs()) {
                skipped++;
                continue;
            }
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            if (!r.decompileCompleted() || r.getHighFunction() == null) {
                skipped++;
                continue;
            }
            if (r.getHighFunction().getFunctionPrototype().getNumParams() == 0) {
                skipped++;
                continue;
            }
            String cc = f.getCallingConventionName();
            HighFunctionDBUtil.commitParamsToDatabase(r.getHighFunction(), true,
                    HighFunctionDBUtil.ReturnCommitOption.NO_COMMIT, SourceType.ANALYSIS);
            if (!cc.equals(f.getCallingConventionName())) f.setCallingConvention(cc);
            stored++;
        }
        di.dispose();
        println("parameters stored: " + stored + " functions, skipped: " + skipped);
    }
}
