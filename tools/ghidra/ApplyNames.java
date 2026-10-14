// Give the 024d functions their names from analysis/024d/functions.csv.
//
// The CSV has one row per function: rva, size, name, source, confidence, evidence. The
// script renames a function only when Ghidra still has its default name (FUN_...), so a
// name that Ghidra knows (a CRT function, a thunk) stays. CRT names (a leading "_") are
// skipped. The decompiler then prints "objecttype_calcquadweights(...)" for a call instead
// of "FUN_00429300(...)". The names are cosmetic for the lifter: lift.py calls each
// callee through its address.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript ApplyNames.java <path of functions.csv>
//@category SubVulpes

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;

public class ApplyNames extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String csv = getScriptArgs()[0];
        int renamed = 0, kept = 0, missing = 0;
        try (BufferedReader in = new BufferedReader(new FileReader(csv))) {
            in.readLine();  // header
            String line;
            while ((line = in.readLine()) != null) {
                List<String> f = split(line);
                if (f.size() < 3 || f.get(2).isEmpty() || f.get(2).startsWith("_")) continue;
                Address a = currentProgram.getImageBase().add(Long.decode(f.get(0)));
                Function fn = getFunctionAt(a);
                if (fn == null) {
                    println("no function at " + a + " for " + f.get(2));
                    missing++;
                    continue;
                }
                if (!fn.getName().startsWith("FUN_")) {
                    if (!fn.getName().equals(f.get(2))) kept++;
                    continue;
                }
                fn.setName(f.get(2), SourceType.USER_DEFINED);
                renamed++;
            }
        }
        println(String.format("renamed %d, kept %d (Ghidra name), missing %d", renamed, kept, missing));
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
