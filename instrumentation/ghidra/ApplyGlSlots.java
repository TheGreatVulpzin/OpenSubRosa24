// Type the OpenGL function pointer slots of the client.
//
// The game calls OpenGL extension functions through a pointer slot (PTR_GLee_Lazy_glXxx) that the
// start-up code fills. Without a type, the decompiler shows "(*(code *)PTR_x)(args)" and loses
// the arguments. This script gives each slot a pointer to a stdcall prototype, so the decompiler
// shows a typed call. instrumentation/client/gen_gl_slots.py reads the same CSV to make the
// C functions that fill the slots in the standalone client.
//
// The CSV has one row per slot: rva, name, cc, prototype.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript ApplyGlSlots.java <path of gl_slots.csv>
//@category SubVulpes

import java.io.BufferedReader;
import java.io.FileReader;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.parser.FunctionSignatureParser;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.data.PointerDataType;

public class ApplyGlSlots extends GhidraScript {
    private static String ghidraTypes(String proto) {
        return proto.replace("unsigned int", "uint").replaceAll("\\*(\\w+)\\(", "* $1(");
    }

    @Override
    protected void run() throws Exception {
        String csv = getScriptArgs()[0];
        FunctionSignatureParser parser = new FunctionSignatureParser(currentProgram.getDataTypeManager(), null);
        int done = 0, failed = 0;
        try (BufferedReader in = new BufferedReader(new FileReader(csv))) {
            in.readLine();  // header
            String line;
            while ((line = in.readLine()) != null) {
                String[] f = line.split(",", 4);
                if (f.length != 4)
                    continue;
                try {
                    Address a = currentProgram.getImageBase().add(Long.decode(f[0]));
                    FunctionDefinitionDataType def = parser.parse(null, ghidraTypes(f[3]));
                    def.setCallingConvention(f[2]);
                    clearListing(a, a.add(3));
                    createData(a, new PointerDataType(def, currentProgram.getDataTypeManager()));
                    println(f[0] + " " + f[1] + ": " + def.getPrototypeString());
                    done++;
                } catch (Exception e) {
                    println("failed " + f[0] + " " + f[1] + ": " + e.getMessage());
                    failed++;
                }
            }
        }
        println("slots typed: " + done + ", failed " + failed);
    }
}
