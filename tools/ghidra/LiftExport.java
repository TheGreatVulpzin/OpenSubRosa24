// Export everything the automatic converter (tools/lift/lift.py) needs, in JSON Lines.
//
// Per function: RVA, name, calling convention, return type, parameters (with the storage of each),
// pseudo-C, typed globals, called functions with signature, and the prologue for the hook
// (bytes, whole instructions >= 5 bytes, positions relocated by ASLR).
//
// Usage (headless, -process <program> -noanalysis): -postScript LiftExport.java <output.jsonl>
//@category SubVulpes

import ghidra.app.decompiler.ClangBreak;
import ghidra.app.decompiler.ClangNode;
import ghidra.app.decompiler.ClangSyntaxToken;
import ghidra.app.decompiler.ClangToken;
import ghidra.app.decompiler.ClangTypeToken;
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
                field(sb, "c", r.decompileCompleted() ? cText(r) : "").append(',');
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

    /* The pseudo-C, built from the token tree, with the bit casts marked.
     *
     * Ghidra prints "(float)x" for two different p-code ops: INT2FLOAT (convert the number)
     * and CAST (keep the bits, one stack slot used as int and as float). C has only the
     * first meaning. A CAST between a 4-byte float and a 4-byte int or pointer becomes
     * LF_BITS_F(x) (bits to float) or (T)LF_BITS_U(x) (float bits to T). The same for
     * 8 bytes and double: LF_BITS_D(x) and (T)LF_BITS_U64(x). Only a cast of a
     * variable or of a parenthesized group is marked. */
    private String cText(DecompileResults r) {
        List<ClangNode> leaves = new ArrayList<>();
        r.getCCodeMarkup().flatten(leaves);
        List<ClangToken> t = new ArrayList<>();
        for (ClangNode n : leaves) t.add((ClangToken) n);
        StringBuilder sb = new StringBuilder();
        Map<Integer, Integer> closeAfter = new HashMap<>();
        for (int i = 0; i < t.size(); i++) {
            ClangToken tok = t.get(i);
            if (tok instanceof ClangBreak) {
                sb.append('\n');
                for (int k = 0; k < ((ClangBreak) tok).getIndent(); k++) sb.append(' ');
                continue;
            }
            int[] cast = bitCast(t, i);
            if (cast != null) {
                // cast = {index of ')' of the cast, operand end index, 0 = to float / 1 = from float}
                String type = castType(t, i, cast[0]);
                // 0: 4 bytes to float, 1: float to 4 bytes, 2: 8 bytes to double, 3: double to 8 bytes
                String[] open = { "LF_BITS_F(", "((" + type + ")LF_BITS_U(", "LF_BITS_D(", "((" + type + ")LF_BITS_U64(" };
                sb.append(open[cast[2]]);
                closeAfter.merge(cast[1], cast[2] % 2 == 0 ? 1 : 2, Integer::sum);
                i = cast[0];
                continue;
            }
            sb.append(tok.getText());
            Integer close = closeAfter.remove(i);
            if (close != null) for (int k = 0; k < close; k++) sb.append(')');
        }
        return sb.toString();
    }

    private static int next(List<ClangToken> t, int i) {
        for (int j = i + 1; j < t.size(); j++) {
            if (t.get(j) instanceof ClangBreak) return -1;
            if (!t.get(j).getText().isBlank()) return j;
        }
        return -1;
    }

    private static String castType(List<ClangToken> t, int open, int close) {
        StringBuilder s = new StringBuilder();
        for (int j = open + 1; j < close; j++) s.append(t.get(j).getText());
        return s.toString().trim();
    }

    private int[] bitCast(List<ClangToken> t, int i) {
        if (!(t.get(i) instanceof ClangSyntaxToken) || !t.get(i).getText().equals("(")) return null;
        int ty = next(t, i);
        if (ty < 0 || !(t.get(ty) instanceof ClangTypeToken)) return null;
        int close = next(t, ty);
        while (close >= 0 && t.get(close).getText().equals("*")) close = next(t, close);
        if (close < 0 || !t.get(close).getText().equals(")")) return null;
        int k = next(t, close);
        if (k < 0) return null;
        ghidra.program.model.pcode.PcodeOp castOp = null;
        int end;
        ClangToken op0 = t.get(k);
        if (op0 instanceof ClangVariableToken) {
            castOp = op0.getPcodeOp();
            end = k;
        } else if (op0.getText().equals("(") || (op0.getPcodeOp() != null && next(t, k) >= 0
                && t.get(next(t, k)).getText().equals("("))) {
            // A parenthesized group, or a call-like operand such as CONCAT44(a, b): the
            // CAST consumes the output of an op inside it.
            int depth = 0;
            end = -1;
            int from = op0.getText().equals("(") ? k : next(t, k);
            if (op0.getPcodeOp() != null && !op0.getText().equals("(")
                    && op0.getPcodeOp().getOutput() != null) {
                Iterator<ghidra.program.model.pcode.PcodeOp> d0 = op0.getPcodeOp().getOutput().getDescendants();
                while (d0.hasNext()) {
                    ghidra.program.model.pcode.PcodeOp q = d0.next();
                    if (q.getOpcode() == ghidra.program.model.pcode.PcodeOp.CAST) castOp = q;
                }
            }
            for (int j = from; j < t.size(); j++) {
                String s = t.get(j).getText();
                if (s.equals("(")) depth++;
                else if (s.equals(")") && --depth == 0) {
                    end = j;
                    break;
                }
            }
            if (end < 0) return null;
            outer:
            for (int j = castOp == null ? k + 1 : end; j < end; j++) {
                ghidra.program.model.pcode.PcodeOp p = t.get(j).getPcodeOp();
                if (p == null || p.getOutput() == null) continue;
                Iterator<ghidra.program.model.pcode.PcodeOp> d = p.getOutput().getDescendants();
                while (d.hasNext()) {
                    ghidra.program.model.pcode.PcodeOp q = d.next();
                    if (q.getOpcode() == ghidra.program.model.pcode.PcodeOp.CAST) {
                        castOp = q;
                        break outer;
                    }
                }
            }
        } else {
            return null;
        }
        if (castOp == null || castOp.getOpcode() != ghidra.program.model.pcode.PcodeOp.CAST) return null;
        if (castOp.getOutput() == null || castOp.getOutput().getHigh() == null || castOp.getInput(0).getHigh() == null)
            return null;
        DataType dst = castOp.getOutput().getHigh().getDataType();
        DataType src = castOp.getInput(0).getHigh().getDataType();
        int len = dst.getLength();
        if (len != src.getLength() || (len != 4 && len != 8)) return null;
        boolean dstF = dst.getName().equals(len == 4 ? "float" : "double");
        boolean srcF = src.getName().equals(len == 4 ? "float" : "double");
        if (dstF == srcF) return null;
        return new int[] { close, end, (len == 4 ? 0 : 2) + (dstF ? 0 : 1) };
    }

    /* Globals: the symbol map, plus every DAT_/_DAT_ token of the C output with the data type
     * the decompiler used for it (an "overlap" global like _DAT_x is not in the symbol map). */
    private void globals(StringBuilder sb, DecompileResults r) {
        Map<String, String[]> found = new LinkedHashMap<>();
        Map<String, Set<String>> valueTypes = new HashMap<>();
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
            // Types of the uses as a value (not "&DAT_x", an address with byte arithmetic).
            int prev = leaves.indexOf(n) - 1;
            while (prev >= 0 && ((ClangToken) leaves.get(prev)).getText().isBlank()) prev--;
            boolean addressOf = prev >= 0 && ((ClangToken) leaves.get(prev)).getText().equals("&");
            if (!addressOf) valueTypes.computeIfAbsent(text, k -> new HashSet<>()).add(
                    dt.getDisplayName() + "|" + dt.getLength());
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
        // A global with the listing type "undefined" (1 byte, no real type) that the function
        // uses as a value of one other type: numofevents is "undefined" but read as a uint.
        // With the 1-byte type, C reads and writes only its low byte.
        // With several use types (numofevents: uint, and ushort for its low half), the largest
        // one wins: the smaller ones are pieces of it.
        for (Map.Entry<String, String[]> e : found.entrySet()) {
            Set<String> uses = valueTypes.get(e.getKey());
            if (!e.getValue()[1].equals("undefined") || uses == null) continue;
            String best = null;
            int bestLen = 1;
            for (String u : uses) {
                String[] tl = u.split("\\|");
                int len = Integer.parseInt(tl[1]);
                if (tl[0].startsWith("undefined") || tl[0].contains("*") || len <= bestLen) continue;
                best = tl[0];
                bestLen = len;
            }
            if (best != null) {
                e.getValue()[1] = best;
                e.getValue()[2] = String.valueOf(bestLen);
            }
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
