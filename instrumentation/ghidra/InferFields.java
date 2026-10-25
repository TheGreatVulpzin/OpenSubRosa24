// Read the type of each field of the typed globals from the instructions that use it.
//
// The decompiler works one function at a time: a function that only copies a field cannot
// tell that another function uses it as a float. This script looks at every instruction of
// the program. For each data reference into a struct global of build/ghidra/types.json it
// writes the offset in the record, the access size and what the instruction shows:
//   float   x87 or SSE float operation on 4 bytes     double  the same on 8 bytes
//   fint    FILD/FIST/FISTP (an integer read or written by the FPU)
//   signed  CMP followed by JL/JG/JLE/JGE/SETL...,  or MOVSX
//   unsigned CMP followed by JB/JA/JBE/JAE/SETB..., or MOVZX
//   int     any other access (MOV, ADD...): an integer or raw bytes
// Output (TSV): global, offset, size, kind, function. tools/re/infer_fields.py turns it into
// analysis/structs/024d/<global>.csv for tools/re/gen_types.py.
//
// Headless: -postScript InferFields.java <types.json> <output.tsv>
// @category SubVulpes
import java.io.FileReader;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.Reference;

public class InferFields extends GhidraScript {

    private static class Region {
        String name;
        long start, end, record;
    }

    @Override
    protected void run() throws Exception {
        String[] a = getScriptArgs();
        String in = a.length > 0 ? a[0] : "build/ghidra/types.json";
        String out = a.length > 1 ? a[1] : "build/re/field_uses.tsv";
        JsonObject root;
        try (FileReader r = new FileReader(in)) {
            root = JsonParser.parseReader(r).getAsJsonObject();
        }
        Map<String, Long> size = new HashMap<>();
        for (JsonElement e : root.getAsJsonArray("structs")) {
            JsonObject s = e.getAsJsonObject();
            size.put(s.get("name").getAsString(), s.get("size").getAsLong());
        }
        long base = currentProgram.getImageBase().getOffset();
        List<Region> regions = new ArrayList<>();
        for (JsonElement e : root.getAsJsonArray("globals")) {
            JsonObject g = e.getAsJsonObject();
            if (g.get("type").isJsonNull()) continue;
            Long rec = size.get(g.get("type").getAsString());
            if (rec == null) continue;  // a base type (int, float...): nothing to infer
            Region r = new Region();
            r.name = g.get("name").getAsString().replaceFirst("^g_", "");
            r.start = base + g.get("rva").getAsLong();
            r.record = rec;
            int count = g.get("count").getAsInt();
            r.end = r.start + rec * Math.max(count, 1);
            regions.add(r);
        }
        int rows = 0;
        try (PrintWriter w = new PrintWriter(out)) {
            InstructionIterator it = currentProgram.getListing().getInstructions(true);
            while (it.hasNext() && !monitor.isCancelled()) {
                Instruction ins = it.next();
                for (Reference ref : ins.getReferencesFrom()) {
                    if (!ref.getReferenceType().isData() && ref.getReferenceType() != RefType.DATA) continue;
                    long to = ref.getToAddress().getOffset();
                    Region r = find(regions, to);
                    if (r == null) continue;
                    int len = accessSize(ins, to);
                    if (len <= 0) continue;  // an address taken (LEA, PUSH imm): not an access
                    long off = (to - r.start) % r.record;
                    Function f = getFunctionContaining(ins.getAddress());
                    w.printf("%s\t0x%x\t%d\t%s\t%s%n", r.name, off, len, kind(ins, len),
                            f == null ? "?" : f.getName());
                    rows++;
                }
            }
        }
        println(String.format("InferFields: %d accesses into %d typed globals -> %s", rows, regions.size(), out));
    }

    private static Region find(List<Region> regions, long a) {
        for (Region r : regions)
            if (a >= r.start && a < r.end) return r;
        return null;
    }

    /* The size of the memory access to address "to", from the p-code of the instruction: a
     * varnode in RAM at that address (MOV EAX, [abs]), else the LOAD or STORE of an indexed
     * operand ([abs + reg*4]). 0 when the instruction only takes the address (LEA, PUSH imm). */
    private int accessSize(Instruction ins, long to) {
        int loadStore = 0;
        for (ghidra.program.model.pcode.PcodeOp p : ins.getPcode()) {
            ghidra.program.model.pcode.Varnode out = p.getOutput();
            if (out != null && out.getAddress().isMemoryAddress() && out.getOffset() == to) return out.getSize();
            for (int k = 0; k < p.getNumInputs(); k++) {
                ghidra.program.model.pcode.Varnode v = p.getInput(k);
                if (v.getAddress().isMemoryAddress() && v.getOffset() == to) return v.getSize();
            }
            if (p.getOpcode() == ghidra.program.model.pcode.PcodeOp.LOAD && out != null) loadStore = out.getSize();
            if (p.getOpcode() == ghidra.program.model.pcode.PcodeOp.STORE) loadStore = p.getInput(2).getSize();
        }
        return loadStore;
    }

    private String kind(Instruction ins, int len) {
        String m = ins.getMnemonicString().toUpperCase();
        if (m.startsWith("FILD") || m.startsWith("FIST") || m.startsWith("FIADD") || m.startsWith("FISUB")
                || m.startsWith("FIMUL") || m.startsWith("FIDIV") || m.startsWith("FICOM"))
            return "fint";
        if (m.startsWith("F") || m.endsWith("SS") && !m.equals("CMPSS") || m.equals("MOVAPS") || m.equals("MOVUPS"))
            return len == 8 ? "double" : "float";
        if (m.endsWith("SD") && m.startsWith("MOV") || m.equals("CVTSD2SS")) return "double";
        if (m.equals("MOVSX")) return "signed";
        if (m.equals("MOVZX")) return "unsigned";
        if (m.equals("CMP") || m.equals("TEST")) {
            Instruction n = ins.getNext();
            if (n != null) {
                String j = n.getMnemonicString().toUpperCase();
                if (j.equals("JL") || j.equals("JG") || j.equals("JLE") || j.equals("JGE") || j.equals("SETL")
                        || j.equals("SETG") || j.equals("SETLE") || j.equals("SETGE") || j.equals("JS") || j.equals("JNS"))
                    return "signed";
                if (j.equals("JB") || j.equals("JA") || j.equals("JBE") || j.equals("JAE") || j.equals("SETB")
                        || j.equals("SETA") || j.equals("SETBE") || j.equals("SETAE") || j.equals("JC") || j.equals("JNC"))
                    return "unsigned";
            }
        }
        if (m.equals("IDIV") || m.equals("IMUL") || m.equals("SAR") || m.equals("CDQ")) return "signed";
        if (m.equals("DIV") || m.equals("MUL") || m.equals("SHR")) return "unsigned";
        return "int";
    }
}
