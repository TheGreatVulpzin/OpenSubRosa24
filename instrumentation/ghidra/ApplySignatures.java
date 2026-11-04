// Give full prototypes to functions of the program from a CSV.
//
// A function that reads more stack arguments than the decompiler found (drawtext reads nine and
// a list of more) makes every caller drop its arguments. The lifted C then calls the original
// with one argument, and the original reads garbage. params.csv only retypes parameters that
// exist. This file gives the whole prototype, with "..." for a variable list.
//
// The CSV has one row per function: rva, name, cc, prototype. The prototype is plain C. The
// last field keeps its commas. The script maps the C names of the basic types to Ghidra names.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript ApplySignatures.java <path of signatures.csv>
//@category SubVulpes

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.parser.FunctionSignatureParser;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.data.ParameterDefinition;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Function.FunctionUpdateType;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.listing.ParameterImpl;
import ghidra.program.model.listing.ReturnParameterImpl;
import ghidra.program.model.symbol.SourceType;

public class ApplySignatures extends GhidraScript {
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
                    rows.put(f[0], f);
            }
        }
        FunctionSignatureParser parser = new FunctionSignatureParser(currentProgram.getDataTypeManager(), null);
        int done = 0, failed = 0;
        for (String[] row : rows.values()) {
            try {
                Address a = currentProgram.getImageBase().add(Long.decode(row[0]));
                Function fn = getFunctionAt(a);
                if (fn == null) {
                    println("no function at " + a);
                    failed++;
                    continue;
                }
                FunctionDefinitionDataType def = parser.parse(null, ghidraTypes(row[3]));
                List<Parameter> params = new ArrayList<>();
                for (ParameterDefinition p : def.getArguments())
                    params.add(new ParameterImpl(p.getName(), p.getDataType(), currentProgram));
                fn.updateFunction(row[2], new ReturnParameterImpl(def.getReturnType(), currentProgram), params,
                        FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.USER_DEFINED);
                fn.setVarArgs(def.hasVarArgs());
                println(String.format("%s %s: %d parameters%s", row[0], fn.getName(), fn.getParameterCount(),
                        def.hasVarArgs() ? " and a variable list" : ""));
                done++;
            } catch (Exception e) {
                println("failed " + row[0] + ": " + e.getMessage());
                failed++;
            }
        }
        println("signatures set: " + done + ", failed " + failed + ", rows " + rows.size());
    }
}
