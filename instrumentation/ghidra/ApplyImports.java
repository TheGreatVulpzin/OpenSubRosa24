// Give prototypes to imported functions of a CSV.
//
// Ghidra knows the prototypes of some Windows imports only. For an import of another DLL (OpenAL,
// SDL, speex, steam_api) it shows "void f(void)" with an unknown calling convention. The decompiler
// then drops the arguments of every call, and the lifted C does not compile (C2197).
//
// The CSV has one row per import: dll, name, cc, prototype. The prototype is plain C. The last
// field keeps its commas. The script maps the C names of the basic types to the names of Ghidra.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript ApplyImports.java <path of imports.csv>
//@category SubVulpes

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.parser.FunctionSignatureParser;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.data.ParameterDefinition;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Function.FunctionUpdateType;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.listing.ParameterImpl;
import ghidra.program.model.listing.ReturnParameterImpl;
import ghidra.program.model.symbol.ExternalLocation;
import ghidra.program.model.symbol.ExternalLocationIterator;
import ghidra.program.model.symbol.ExternalManager;
import ghidra.program.model.symbol.SourceType;

public class ApplyImports extends GhidraScript {
    private static String ghidraTypes(String proto) {
        // "void *name(" has no space between the star and the name: the parser reads "*name".
        return proto.replace("unsigned long long", "ulonglong").replace("unsigned int", "uint")
                .replace("unsigned char", "uchar").replace("unsigned short", "ushort")
                .replaceAll("\\*(\\w+)\\(", "* $1(");
    }

    @Override
    protected void run() throws Exception {
        String csv = getScriptArgs()[0];
        Map<String, String[]> rows = new HashMap<>();
        try (BufferedReader in = new BufferedReader(new FileReader(csv))) {
            in.readLine();  // header
            String line;
            while ((line = in.readLine()) != null) {
                String[] f = line.split(",", 4);
                if (f.length == 4)
                    rows.put(f[0].toUpperCase() + "!" + f[1], f);
            }
        }
        FunctionSignatureParser parser = new FunctionSignatureParser(currentProgram.getDataTypeManager(), null);
        ExternalManager em = currentProgram.getExternalManager();
        int done = 0, failed = 0;
        for (String lib : em.getExternalLibraryNames()) {
            ExternalLocationIterator it = em.getExternalLocations(lib);
            while (it.hasNext()) {
                ExternalLocation loc = it.next();
                String[] row = rows.get(lib.toUpperCase() + "!" + loc.getLabel());
                if (row == null)
                    continue;
                try {
                    Function fn = loc.getFunction();
                    if (fn == null)
                        fn = loc.createFunction();
                    FunctionDefinitionDataType def = parser.parse(null, ghidraTypes(row[3]));
                    List<Parameter> params = new ArrayList<>();
                    for (ParameterDefinition p : def.getArguments())
                        params.add(new ParameterImpl(p.getName(), p.getDataType(), currentProgram));
                    fn.updateFunction(row[2], new ReturnParameterImpl(def.getReturnType(), currentProgram), params,
                            FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.USER_DEFINED);
                    done++;
                } catch (Exception e) {
                    println("failed " + lib + "!" + loc.getLabel() + ": " + e.getMessage());
                    failed++;
                }
            }
        }
        println("import prototypes set: " + done + ", failed " + failed + ", rows " + rows.size());
    }
}
