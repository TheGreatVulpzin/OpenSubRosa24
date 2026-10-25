// Lock the return type of the functions that keep the x87 stack (__cdecl_keep*F).
//
// A function with a __cdecl_keep*F convention does not write the x87 stack, so it cannot
// return a float in ST0. The prototype still lists ST0 as an output. When a caller keeps a
// value on the x87 stack across the call and uses it later, the decompiler reads that
// value as the return of the call: "fVar28 = (float10)FUN_00407e50(...)". The lifted code
// then reads an empty ST0.
//
// For each such function, the script locks the return that the decompiler finds in the
// function itself: void when it returns nothing, int when it returns 4 bytes (EAX).
// The parameters must be stored first (CommitParams.java, FixReturnTypes.java): a locked
// signature with no stored parameters makes every caller drop its arguments.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript FixKeepFReturns.java
//@category SubVulpes

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.IntegerDataType;
import ghidra.program.model.data.VoidDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class FixKeepFReturns extends GhidraScript {

    @Override
    protected void run() throws Exception {
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        int toVoid = 0, toInt = 0, skipped = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            String cc = f.getCallingConventionName();
            if (!cc.startsWith("__cdecl_keep") || !cc.endsWith("F")) continue;
            if (f.getSignatureSource() == SourceType.USER_DEFINED) {
                skipped++;  // FixReturnTypes.java already set it
                continue;
            }
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            if (!r.decompileCompleted() || r.getHighFunction() == null) {
                skipped++;
                continue;
            }
            int params = r.getHighFunction().getFunctionPrototype().getNumParams();
            if (params != f.getParameterCount()) {
                println(String.format("%s: %d parameters stored, %d found, skipped", f.getName(),
                        f.getParameterCount(), params));
                skipped++;
                continue;
            }
            DataType ret = r.getHighFunction().getFunctionPrototype().getReturnType();
            if (ret == null || ret.getLength() <= 0 || ret instanceof VoidDataType) {
                f.setReturnType(VoidDataType.dataType, SourceType.USER_DEFINED);
                toVoid++;
            } else if (ret.getLength() == 4 && !ret.getName().equals("float")) {
                f.setReturnType(IntegerDataType.dataType, SourceType.USER_DEFINED);
                toInt++;
            } else {
                println(String.format("%s: return %s, skipped", f.getName(), ret.getName()));
                skipped++;
            }
        }
        di.dispose();
        println("keepF returns: void " + toVoid + ", int " + toInt + ", skipped " + skipped);
    }
}
