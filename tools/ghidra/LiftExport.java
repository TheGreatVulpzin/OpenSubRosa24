// Export everything the automatic converter (tools/lift/lift.py) needs, in JSON Lines.
//
// Per function: RVA, name, calling convention, return type, parameters (with the storage of each),
// pseudo-C, typed globals, called functions with signature, and the prologue for the hook
// (bytes, whole instructions >= 5 bytes, positions relocated by ASLR).
//
// Usage (headless, -process <program> -noanalysis): -postScript LiftExport.java <output.jsonl>
//@category SubVulpes

import ghidra.app.decompiler.ClangNode;
import ghidra.app.decompiler.ClangToken;
import ghidra.app.decompiler.ClangVariableToken;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighSymbol;
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.reloc.Relocation;

import java.io.PrintWriter;
import java.util.*;

public class LiftExport extends GhidraScript {

    private long base;

    @Override
    protected void run() throws Exception {
        String out = getScriptArgs()[0];
        base = currentProgram.getImageBase().getOffset();

        DecompInterface di = new DecompInterface();
        DecompileOptions opts = new DecompileOptions();
        di.setOptions(opts);
        di.openProgram(currentProgram);

        try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
            for (Function f : currentProgram.getListing().getFunctions(true)) {
                if (monitor.isCancelled()) break;
                if (f.isThunk() || f.isExternal()) continue;
                DecompileResults r = di.decompileFunction(f, 120, monitor);
                StringBuilder sb = new StringBuilder("{");
                field(sb, "rva", hex(f.getEntryPoint())).append(',');
                field(sb, "name", f.getName()).append(',');
                field(sb, "cc", f.getCallingConventionName()).append(',');
                field(sb, "ret", f.getReturnType().getDisplayName()).append(',');
                sb.append("\"varargs\":").append(f.hasVarArgs()).append(',');
                sb.append("\"params\":[");
                Parameter[] ps = f.getParameters();
                for (int i = 0; i < ps.length; i++) {
                    if (i > 0) sb.append(',');
                    sb.append('{');
                    field(sb, "name", ps[i].getName()).append(',');
                    field(sb, "type", ps[i].getDataType().getDisplayName()).append(',');
                    field(sb, "storage", ps[i].getVariableStorage().toString());
                    sb.append('}');
                }
                sb.append("],");
                field(sb, "ok", String.valueOf(r.decompileCompleted())).append(',');
                field(sb, "c", r.decompileCompleted() ? r.getDecompiledFunction().getC() : "").append(',');
                sb.append("\"globals\":[");
                if (r.decompileCompleted()) globals(sb, r);
                sb.append("],\"calls\":[");
                calls(sb, f);
                sb.append("],");
                prologue(sb, f);
                sb.append(',');
                relocValues(sb, f);
                sb.append('}');
                w.println(sb);
            }
        }
        di.dispose();
        println("Exported: " + out);
    }

    /* Globals: the symbol map, plus every DAT_/_DAT_ token of the C output with the data type
     * the decompiler used for it (an "overlap" global like _DAT_x is not in the symbol map). */
    private void globals(StringBuilder sb, DecompileResults r) {
        Map<String, String[]> found = new LinkedHashMap<>();
        Iterator<HighSymbol> it = r.getHighFunction().getGlobalSymbolMap().getSymbols();
        while (it.hasNext()) {
            HighSymbol s = it.next();
            Address a = s.getStorage().isMemoryStorage() ? s.getStorage().getMinAddress() : null;
            if (a == null) continue;
            DataType dt = s.getDataType();
            found.put(s.getName(), new String[] { String.format("0x%x", a.getOffset()), dt.getDisplayName(),
                    String.valueOf(dt.getLength()) });
        }
        List<ClangNode> leaves = new ArrayList<>();
        r.getCCodeMarkup().flatten(leaves);
        for (ClangNode n : leaves) {
            if (!(n instanceof ClangVariableToken)) continue;
            String text = ((ClangToken) n).getText();
            HighVariable hv = ((ClangVariableToken) n).getHighVariable();
            if (hv == null) continue;
            DataType dt = hv.getDataType();
            // The symbol map keeps the listing type (often "undefined4"). The decompiler can use
            // the global as a float. In C, "undefined4 x = 1.0" stores the int 1, so the type that
            // the decompiler used wins when it has the same size.
            String[] known = found.get(text);
            if (known != null) {
                if (known[1].startsWith("undefined") && !dt.getDisplayName().startsWith("undefined")
                        && String.valueOf(dt.getLength()).equals(known[2])) {
                    known[1] = dt.getDisplayName();
                }
                continue;
            }
            // DAT_107575c0, _DAT_107575c0, DAT_107575c0_1 (a second symbol at the same address)
            // and PTR_s_Goldmen_Inc_00460438 (a pointer global named after its target).
            java.util.regex.Matcher gm = java.util.regex.Pattern
                    .compile("_*(?:DAT|PTR)_(?:\\w*_)?([0-9a-f]{8})(?:_\\d+)?").matcher(text);
            if (!gm.matches()) continue;
            // The real address: "DAT_107575c0_1" can be a piece at 0x107575c1.
            String hexAddr = gm.group(1);
            ghidra.program.model.pcode.Varnode rep = hv.getRepresentative();
            if (rep != null && rep.getAddress().isMemoryAddress()) {
                hexAddr = String.format("%08x", rep.getAddress().getOffset());
            }
            found.put(text, new String[] { "0x" + hexAddr, dt.getDisplayName(), String.valueOf(dt.getLength()) });
        }
        boolean first = true;
        for (Map.Entry<String, String[]> e : found.entrySet()) {
            if (!first) sb.append(',');
            first = false;
            sb.append('{');
            field(sb, "name", e.getKey()).append(',');
            field(sb, "va", e.getValue()[0]).append(',');
            field(sb, "type", e.getValue()[1]).append(',');
            sb.append("\"size\":").append(e.getValue()[2]);
            sb.append('}');
        }
    }

    private void calls(StringBuilder sb, Function f) {
        boolean first = true;
        for (Function c : f.getCalledFunctions(monitor)) {
            Function t = c.isThunk() ? c.getThunkedFunction(true) : c;
            if (!first) sb.append(',');
            first = false;
            sb.append('{');
            field(sb, "name", c.getName()).append(',');
            field(sb, "target", t.getName()).append(',');
            sb.append("\"external\":").append(t.isExternal()).append(',');
            if (t.isExternal()) {
                String lib = t.getExternalLocation() != null ? t.getExternalLocation().getLibraryName() : "";
                field(sb, "library", lib).append(',');
            } else {
                field(sb, "rva", hex(t.getEntryPoint())).append(',');
            }
            field(sb, "cc", t.getCallingConventionName()).append(',');
            field(sb, "proto", t.getSignature().getPrototypeString());
            sb.append('}');
        }
    }

    /* Prologue: whole instructions up to >= 5 bytes. Flags a relative jump and relocated positions. */
    private void prologue(StringBuilder sb, Function f) throws Exception {
        Instruction ins = getInstructionAt(f.getEntryPoint());
        int len = 0;
        boolean relative = false;
        while (ins != null && len < 5) {
            if (ins.getFlowType().isJump() || ins.getFlowType().isCall()) relative = true;
            len += ins.getLength();
            ins = ins.getNext();
        }
        byte[] bytes = new byte[len];
        currentProgram.getMemory().getBytes(f.getEntryPoint(), bytes);
        StringBuilder hex = new StringBuilder();
        for (byte b : bytes) hex.append(String.format("%02x", b));
        sb.append("\"prologue\":{\"len\":").append(len).append(",\"relative\":").append(relative).append(',');
        field(sb, "bytes", hex.toString()).append(",\"relocs\":[");
        boolean first = true;
        Iterator<Relocation> it = currentProgram.getRelocationTable().getRelocations(
                new ghidra.program.model.address.AddressSet(f.getEntryPoint(), f.getEntryPoint().add(len - 1)));
        while (it.hasNext()) {
            Relocation rel = it.next();
            if (!first) sb.append(',');
            first = false;
            sb.append(rel.getAddress().subtract(f.getEntryPoint()));
        }
        sb.append("]}");
    }

    /* Values at relocation sites inside the function: absolute addresses that the loader
     * moves with ASLR. The pseudo-C shows some of them as plain numbers. */
    private void relocValues(StringBuilder sb, Function f) throws Exception {
        TreeSet<Long> vals = new TreeSet<>();
        Iterator<Relocation> it = currentProgram.getRelocationTable().getRelocations(f.getBody());
        while (it.hasNext()) {
            Relocation rel = it.next();
            vals.add(currentProgram.getMemory().getInt(rel.getAddress()) & 0xffffffffL);
        }
        sb.append("\"relocvals\":[");
        boolean first = true;
        for (long v : vals) {
            if (!first) sb.append(',');
            first = false;
            sb.append(v);
        }
        sb.append(']');
    }

    private String hex(Address a) {
        return String.format("0x%x", a.getOffset() - base);
    }

    private static StringBuilder field(StringBuilder sb, String k, String v) {
        sb.append('"').append(k).append("\":\"");
        for (char ch : v.toCharArray()) {
            if (ch == '"' || ch == '\\') sb.append('\\').append(ch);
            else if (ch == '\n') sb.append("\\n");
            else if (ch == '\r') continue;
            else if (ch == '\t') sb.append("\\t");
            else if (ch < 0x20) sb.append(String.format("\\u%04x", (int) ch));
            else sb.append(ch);
        }
        return sb.append('"');
    }
}
