// Give an int return type to the functions of a CSV.
//
// A function that Ghidra types as void can still return a value in EAX: it ends with a
// call to a function whose return the decompiler does not know, or its callers read EAX.
// The callers then take a value from a "void" function, and the lifted C does not compile
// (C2069, C2186).
//
// The CSV has one row per function: rva, name, type, evidence. Only the type "int" is
// supported. The script stores the parameters that the decompiler finds, then sets the
// return type. A locked signature with no stored parameters makes every caller drop its
// arguments, so the parameters come first.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript ApplyReturns.java <path of returns.csv>
//@category SubVulpes

import java.io.BufferedReader;
import java.io.FileReader;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.IntegerDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunctionDBUtil;
import ghidra.program.model.symbol.SourceType;

public class ApplyReturns extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String csv = getScriptArgs()[0];
        int done = 0, missing = 0, skipped = 0;
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        try (BufferedReader in = new BufferedReader(new FileReader(csv))) {
            in.readLine();  // header
            String line;
            while ((line = in.readLine()) != null) {
                String[] f = line.split(",", 4);
                if (f.length < 3 || !f[2].equals("int")) {
                    skipped++;
                    continue;
                }
                Address a = currentProgram.getImageBase().add(Long.decode(f[0]));
                Function fn = getFunctionAt(a);
                if (fn == null) {
                    println("no function at " + a);
                    missing++;
                    continue;
                }
                fn.setSignatureSource(SourceType.DEFAULT);
                DecompileResults r = di.decompileFunction(fn, 120, monitor);
                if (r.decompileCompleted()) {
                    HighFunctionDBUtil.commitParamsToDatabase(r.getHighFunction(), true,
                            HighFunctionDBUtil.ReturnCommitOption.NO_COMMIT, SourceType.USER_DEFINED);
                    fn.setReturnType(IntegerDataType.dataType, SourceType.USER_DEFINED);
                    println(String.format("%s %s: int, %d parameters", f[0], fn.getName(), fn.getParameterCount()));
                    done++;
                }
            }
        }
        di.dispose();
        println("int returns set: " + done + ", missing " + missing + ", skipped " + skipped);
    }
}
