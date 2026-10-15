// Give 024d function parameters their types from analysis/024d/params.csv.
//
// The CSV has one row per parameter: rva, name, index (0-based), type, evidence. Ghidra often
// types a pointer parameter as int or undefined4. The lifted table (lift.py) then marks the
// argument as a value ('v'), and the harness skips each recorded call where the value points
// into the caller stack. As a pointer ('p'), the harness copies the recorded memory and runs
// the call. The callers also lose their (int) casts.
//
// The script prints the old type of each parameter, so a change can be undone by hand.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript ApplyParams.java <path of params.csv>
//@category SubVulpes

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.symbol.SourceType;
import ghidra.util.data.DataTypeParser;
import ghidra.util.data.DataTypeParser.AllowedDataTypes;

public class ApplyParams extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String csv = getScriptArgs()[0];
        DataTypeParser parser = new DataTypeParser(currentProgram.getDataTypeManager(), null, null,
                AllowedDataTypes.ALL);
        int changed = 0, same = 0, failed = 0;
        try (BufferedReader in = new BufferedReader(new FileReader(csv))) {
            in.readLine();  // header
            String line;
            while ((line = in.readLine()) != null) {
                List<String> f = split(line);
                if (f.size() < 4) continue;
                Address a = currentProgram.getImageBase().add(Long.decode(f.get(0)));
                Function fn = getFunctionAt(a);
                int index = Integer.parseInt(f.get(2));
                if (fn == null || index >= fn.getParameterCount()) {
                    println("no function or parameter " + index + " at " + a + " for " + f.get(1));
                    failed++;
                    continue;
                }
                Parameter p = fn.getParameter(index);
                DataType dt = parser.parse(f.get(3));
                if (p.getDataType().isEquivalent(dt)) {
                    same++;
                    continue;
                }
                println(String.format("%s param %d: %s -> %s", fn.getName(), index, p.getDataType().getDisplayName(),
                        dt.getDisplayName()));
                p.setDataType(dt, SourceType.USER_DEFINED);
                changed++;
            }
        }
        println(String.format("changed %d, already right %d, failed %d", changed, same, failed));
    }

    /* One CSV line: fields separated by commas, a field in double quotes can hold commas. */
    private static List<String> split(String line) {
        List<String> out = new ArrayList<>();
        StringBuilder cur = new StringBuilder();
        boolean quoted = false;
        for (int i = 0; i < line.length(); i++) {
            char c = line.charAt(i);
            if (c == '"') {
                if (quoted && i + 1 < line.length() && line.charAt(i + 1) == '"') {
                    cur.append('"');
                    i++;
                } else {
                    quoted = !quoted;
                }
            } else if (c == ',' && !quoted) {
                out.add(cur.toString());
                cur.setLength(0);
            } else {
                cur.append(c);
            }
        }
        out.add(cur.toString());
        return out;
    }
}
