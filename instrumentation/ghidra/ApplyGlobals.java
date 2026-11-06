// Give a type to global variables of the program from a CSV.
//
// A global that holds a pointer (the result of malloc) shows as "undefined4" in the pseudo-C.
// The lifted C then dereferences an integer and does not compile. The CSV types the global, so
// the decompiler shows "*DAT_x" and "DAT_x[i]".
//
// The CSV has one row per global: rva, name, type, count, evidence. The type is plain C
// ("uint *", "float", "char"). A count above 1 makes an array. The name column is for people:
// the script keeps the label of the program.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript ApplyGlobals.java <path of data_globals.csv>
//@category SubVulpes

import java.io.BufferedReader;
import java.io.FileReader;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.DataType;
import ghidra.util.data.DataTypeParser;
import ghidra.util.data.DataTypeParser.AllowedDataTypes;

public class ApplyGlobals extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String csv = getScriptArgs()[0];
        DataTypeParser parser = new DataTypeParser(currentProgram.getDataTypeManager(), null, null,
                AllowedDataTypes.ALL);
        int done = 0, failed = 0;
        try (BufferedReader in = new BufferedReader(new FileReader(csv))) {
            in.readLine();  // header
            String line;
            while ((line = in.readLine()) != null) {
                String[] f = line.split(",", 5);
                if (f.length < 4)
                    continue;
                try {
                    Address a = currentProgram.getImageBase().add(Long.decode(f[0]));
                    DataType type = parser.parse(f[2].trim());
                    int count = Integer.parseInt(f[3].trim());
                    DataType full = count > 1 ? new ArrayDataType(type, count, type.getLength()) : type;
                    clearListing(a, a.add(full.getLength() - 1));
                    createData(a, full);
                    println(f[0] + " " + f[1] + ": " + full.getName());
                    done++;
                } catch (Exception e) {
                    println("failed " + f[0] + " " + f[1] + ": " + e.getMessage());
                    failed++;
                }
            }
        }
        println("globals typed: " + done + ", failed " + failed);
    }
}
