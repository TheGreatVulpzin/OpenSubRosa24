// Give the MSVC x87 intrinsics (_CIsqrt, _CIsin, _CIatan2...) their real signatures.
//
// These CRT functions take their arguments on the x87 stack, not on the CPU stack:
// one argument in ST0, or two arguments in ST1 (first) and ST0 (second). The result
// comes back in ST0. Without this, the decompiler prints "_CIsqrt();" and loses the argument.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript FixX87Intrinsics.java
//@category SubVulpes

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.DoubleDataType;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.SourceType;

import java.util.*;

public class FixX87Intrinsics extends GhidraScript {

    private static final Set<String> ONE_ARG = new HashSet<>(Arrays.asList(
            "_CIsqrt", "_CIsin", "_CIcos", "_CItan", "_CIacos", "_CIasin", "_CIatan", "_CIexp", "_CIlog",
            "_CIlog10", "_CIsinh", "_CIcosh", "_CItanh"));
    private static final Set<String> TWO_ARGS = new HashSet<>(Arrays.asList("_CIatan2", "_CIpow", "_CIfmod"));

    @Override
    protected void run() throws Exception {
        Register st0 = currentProgram.getRegister("ST0");
        Register st1 = currentProgram.getRegister("ST1");
        int fixed = 0;
        List<Function> targets = new ArrayList<>();
        for (Function f : currentProgram.getFunctionManager().getExternalFunctions()) targets.add(f);
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            if (f.isThunk()) targets.add(f);
        }
        for (Function f : targets) {
            String name = f.getName();
            if (!ONE_ARG.contains(name) && !TWO_ARGS.contains(name)) continue;
            DoubleDataType dbl = DoubleDataType.dataType;
            ReturnParameterImpl ret = new ReturnParameterImpl(dbl, new VariableStorage(currentProgram, st0),
                    currentProgram);
            List<Variable> params = new ArrayList<>();
            if (ONE_ARG.contains(name)) {
                params.add(new ParameterImpl("x", dbl, new VariableStorage(currentProgram, st0), currentProgram));
            } else {
                params.add(new ParameterImpl("a", dbl, new VariableStorage(currentProgram, st1), currentProgram));
                params.add(new ParameterImpl("b", dbl, new VariableStorage(currentProgram, st0), currentProgram));
            }
            f.updateFunction(null, ret, params, Function.FunctionUpdateType.CUSTOM_STORAGE, true,
                    SourceType.USER_DEFINED);
            println("fixed " + name + (f.isExternal() ? " (external)" : " (thunk)"));
            fixed++;
        }
        println("x87 intrinsics fixed: " + fixed);
    }
}
