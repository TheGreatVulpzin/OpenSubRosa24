// Export everything the automatic converter (tools/lift/lift.py) needs, in JSON Lines.
//
// Per function: RVA, name, calling convention, return type, parameters (with the storage of each),
// pseudo-C, typed globals, called functions with signature, and the prologue for the hook
// (bytes, whole instructions >= 5 bytes, positions relocated by ASLR).
//
// Usage (headless, -process <program> -noanalysis): -postScript LiftExport.java <output.jsonl>
//@category SubVulpes

import ghidra.app.decompiler.ClangBreak;
import ghidra.app.decompiler.ClangFuncNameToken;
import ghidra.app.decompiler.ClangNode;
import ghidra.app.decompiler.ClangOpToken;
import ghidra.app.decompiler.ClangStatement;
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
import ghidra.program.model.data.Pointer;
import ghidra.program.model.data.Undefined;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighSymbol;
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.Varnode;
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
        if (protoDi != null) protoDi.dispose();
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
        Map<Integer, String> openBefore = new HashMap<>();
        // A rule that runs later puts its "(" outside the ones before it at the same token:
        // rounding and grouping go inside, the bit copies of callArgBits outside.
        collectRdataFloats(r.getFunction());
        floatStores(t, openBefore, closeAfter);
        storeRounding(t, openBefore, closeAfter);
        floatGrouping(t, openBefore, closeAfter);
        callArgBits(t, openBefore, closeAfter);
        pieceArgBits(t, openBefore, closeAfter);
        replaceSpans.clear();
        floatBitCompares(t, openBefore, closeAfter);
        try {
            relocatedConstants(t, r.getFunction());
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        expressionPieces(t, openBefore);
        for (int i = 0; i < t.size(); i++) {
            ClangToken tok = t.get(i);
            if (tok instanceof ClangBreak) {
                sb.append('\n');
                for (int k = 0; k < ((ClangBreak) tok).getIndent(); k++) sb.append(' ');
                continue;
            }
            String lhsOpen = openBefore.get(i);
            if (lhsOpen != null) sb.append(lhsOpen);
            Object[] rep = replaceSpans.get(i);
            if (rep != null) {
                // A constant printed as a float literal, replaced by its bits.
                int repEnd = (Integer) rep[0];
                sb.append((String) rep[1]);
                for (int k = i; k <= repEnd; k++) {
                    Integer close = closeAfter.remove(k);
                    if (close != null) for (int c = 0; c < close; c++) sb.append(')');
                }
                i = repEnd;
                continue;
            }
            String piece = partialAccess(t, i);
            if (piece != null) {
                sb.append(piece);
                for (int k = i; k <= pieceEnd; k++) {
                    Integer close = closeAfter.remove(k);
                    if (close != null) for (int c = 0; c < close; c++) sb.append(')');
                }
                i = pieceEnd;
                continue;
            }
            String smaller = pieceVariable(t, i);
            if (smaller != null) {
                sb.append(smaller);
                Integer close = closeAfter.remove(i);
                if (close != null) for (int c = 0; c < close; c++) sb.append(')');
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
            // An unknown area of a struct (ApplyTypes.java leaves it without a field): Ghidra
            // prints "&x->field_0x90". In C each struct is a union with lf_raw, its raw bytes.
            if (tok.getText().equals("ROUND") && choppedRound(tok)) {
                sb.append("LF_CHOP");
                Integer close = closeAfter.remove(i);
                if (close != null) for (int k = 0; k < close; k++) sb.append(')');
                continue;
            }
            String exact = exactFloat(tok);
            if (tok instanceof ghidra.app.decompiler.ClangFieldToken && RAW_FIELD.matcher(tok.getText()).matches())
                sb.append("lf_raw[").append(tok.getText().substring(6)).append(']');
            else if (exact != null)
                sb.append(exact);
            else
                sb.append(tok.getText());
            Integer close = closeAfter.remove(i);
            if (close != null) for (int k = 0; k < close; k++) sb.append(')');
        }
        return sb.toString();
    }

    /* A FISTP in truncation mode. MSVC converts a float to an int with FNSTCW, OR 0xc00 (the
     * rounding control "chop"), FLDCW, FISTP, then FLDCW of the old word. Ghidra prints the
     * FISTP as ROUND(x), and the FLDCW is lost: in C, ROUND is rint() and rounds to nearest.
     * serversend packed the view angle one unit too high. Returns true when the ROUND token
     * comes from a FIST/FISTP that a FLDCW of a word with "OR 0xc00" comes before, in the
     * same run of instructions. */
    private boolean choppedRound(ClangToken tok) {
        PcodeOp op = tok.getPcodeOp();
        if (op == null || op.getOpcode() != PcodeOp.FLOAT_ROUND) return false;
        Instruction ins = currentProgram.getListing().getInstructionAt(op.getSeqnum().getTarget());
        if (ins == null || !ins.getMnemonicString().startsWith("FIST")) return false;
        boolean fldcw = false;
        for (int n = 0; n < 16 && ins != null; n++) {
            ins = currentProgram.getListing().getInstructionBefore(ins.getAddress());
            if (ins == null || ins.getFlowType().isJump() || ins.getFlowType().isCall()) return false;
            String m = ins.getMnemonicString();
            if (m.equals("FLDCW")) {
                if (fldcw) return false;  // the restore of an earlier conversion
                fldcw = true;
            } else if (fldcw && m.equals("OR") && ins.getNumOperands() == 2
                    && ins.getScalar(1) != null && (ins.getScalar(1).getUnsignedValue() & 0xc00) == 0xc00) {
                return true;
            }
        }
        return false;
    }

    /* A float constant printed with all its digits. Ghidra prints a short form: serversend
     * multiplies by the double 5215.189175234883 (32768 / 2 pi), and Ghidra prints 5215.189.
     * In C the short form is another number, and the packed angle was off by a few units. The
     * value is the exact float or double of the constant, printed as a double literal (as
     * Ghidra does), so the type of the expression does not change. Returns null when the
     * token is not a float constant. */
    private static final java.util.regex.Pattern DECIMAL = java.util.regex.Pattern.compile("^-?\\d+\\.\\d*([eE][-+]?\\d+)?$|^-?\\d+[eE][-+]?\\d+$");

    /* The float constants that the function reads from .rdata: the 4-byte floats, and the
     * 8-byte doubles. Ghidra can make a 4-byte constant from a double: packangle divides by
     * the double 6.28318530718 (FDIV double ptr), and the p-code constant is the float
     * 6.2831855. Filled by cText for each function. */
    private final Set<Float> rdataFloats = new HashSet<>();
    private final Map<Float, Set<Double>> rdataDoubles = new HashMap<>();

    private void collectRdataFloats(Function f) {
        rdataFloats.clear();
        rdataDoubles.clear();
        ghidra.program.model.mem.MemoryBlock rdata = currentProgram.getMemory().getBlock(".rdata");
        if (f == null || rdata == null) return;
        for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
            if (!ins.getMnemonicString().startsWith("F")) continue;
            for (ghidra.program.model.symbol.Reference ref : ins.getReferencesFrom()) {
                Address a = ref.getToAddress();
                if (!rdata.contains(a)) continue;
                try {
                    // The operand size: "float ptr" or "double ptr".
                    String text = ins.toString();
                    if (text.contains("double ptr")) {
                        double d = Double.longBitsToDouble(currentProgram.getMemory().getLong(a));
                        rdataDoubles.computeIfAbsent((float) d, k -> new HashSet<>()).add(d);
                    } else if (text.contains("float ptr")) {
                        rdataFloats.add(Float.intBitsToFloat(currentProgram.getMemory().getInt(a)));
                    }
                } catch (ghidra.program.model.mem.MemoryAccessException e) {
                    // not initialized: no constant
                }
            }
        }
    }

    private String exactFloat(ClangToken tok) {
        if (!(tok instanceof ClangVariableToken) || !DECIMAL.matcher(tok.getText()).matches()) return null;
        Varnode vn = ((ClangVariableToken) tok).getVarnode();
        if (vn == null || !vn.isConstant() || vn.getHigh() == null) return null;
        int len = vn.getSize();
        if (!isFloat(vn.getHigh().getDataType(), len)) return null;
        double d;
        if (len == 4) {
            float c = Float.intBitsToFloat((int) vn.getOffset());
            d = c;
            // A float that the function never reads as a float, but reads as one double.
            Set<Double> wide = rdataDoubles.get(Math.abs(c));
            if (!rdataFloats.contains(Math.abs(c)) && wide != null && wide.size() == 1)
                d = Math.copySign(wide.iterator().next(), c);
        } else if (len == 8) {
            d = Double.longBitsToDouble(vn.getOffset());
        } else {
            return null;
        }
        if (Double.isNaN(d) || Double.isInfinite(d)) return null;
        String s = Double.toString(d);
        // Keep the printed sign: "-x" can be a unary minus token before a positive constant.
        if (tok.getText().startsWith("-") != (d < 0)) return null;
        return s;
    }

    private static final java.util.regex.Pattern FIELD = java.util.regex.Pattern.compile("^_(\\d+)_(\\d+)_$");
    private static final java.util.regex.Pattern RAW_FIELD = java.util.regex.Pattern.compile("^field_0x[0-9a-f]+$");

    /* Ghidra "x._4_2_" is 2 bytes at offset 4 of x. Print it as a pointer access, with the
     * type that the decompiler gives to the piece: float when the piece is a float, so that
     * "local_64 = local_5c._0_4_;" copies the bits of a float and does not convert an int.
     * Returns null when the tokens at i are not "variable . _N_M_". */
    private int pieceEnd;

    private String partialAccess(List<ClangToken> t, int i) {
        if (!(t.get(i) instanceof ClangVariableToken)) return null;
        int dot = next(t, i), field = dot < 0 ? -1 : next(t, dot);
        if (field < 0 || !t.get(dot).getText().equals(".")) return null;
        java.util.regex.Matcher m = FIELD.matcher(t.get(field).getText());
        if (!m.find()) return null;
        int off = Integer.parseInt(m.group(1)), len = Integer.parseInt(m.group(2));
        pieceEnd = field;
        return "(*(" + pieceType(t, dot, len) + " *)((uchar *)&" + t.get(i).getText() + " + " + off + "))";
    }

    /* The same for a piece of an expression: "g_itemtype[8]._40_4_" (an unknown area of a
     * struct, ApplyTypes.java leaves it without a field), "x.f._0_4_" or "p->f._0_4_". The
     * expression starts where its postfix chain (names, [..], .field, ->field) starts. */
    private void expressionPieces(List<ClangToken> t, Map<Integer, String> openBefore) {
        for (int dot = 1; dot + 1 < t.size(); dot++) {
            if (!t.get(dot).getText().equals(".") || replaceSpans.containsKey(dot)) continue;
            int field = next(t, dot);
            if (field < 0) continue;
            java.util.regex.Matcher m = FIELD.matcher(t.get(field).getText());
            if (!m.find()) continue;
            int prev = prevTok(t, dot);
            if (prev < 0 || t.get(prev) instanceof ClangVariableToken && prevTok(t, prev) >= 0
                    && !isChain(t.get(prevTok(t, prev)).getText()))
                continue;  // "var._N_M_": partialAccess
            int start = postfixStart(t, prev);
            if (start < 0) continue;
            int off = Integer.parseInt(m.group(1)), len = Integer.parseInt(m.group(2));
            openBefore.merge(start, "(*(" + pieceType(t, dot, len) + " *)((uchar *)&", (x, y) -> y + x);
            replaceSpans.put(dot, new Object[] { field, " + " + off + "))" });
        }
    }

    private static boolean isChain(String s) {
        return s.equals(".") || s.equals("->") || s.equals("]");
    }

    private static int prevTok(List<ClangToken> t, int i) {
        for (int j = i - 1; j >= 0; j--)
            if (!t.get(j).getText().isBlank() && !(t.get(j) instanceof ClangBreak)) return j;
        return -1;
    }

    /* The last token of the postfix chain that starts at the name t[start]: "a[i].f->g". */
    private static int postfixEnd(List<ClangToken> t, int start) {
        int end = start;
        while (true) {
            int n = next(t, end);
            if (n < 0) return end;
            String s = t.get(n).getText();
            if (s.equals("[")) {
                int depth = 0, j;
                for (j = n; j < t.size(); j++) {
                    String u = t.get(j).getText();
                    if (u.equals("[")) depth++;
                    else if (u.equals("]") && --depth == 0) break;
                }
                if (j >= t.size()) return end;
                end = j;
            } else if ((s.equals(".") || s.equals("->")) && next(t, n) >= 0
                    && t.get(next(t, n)) instanceof ghidra.app.decompiler.ClangFieldToken) {
                end = next(t, n);
            } else {
                return end;
            }
        }
    }

    /* The first token of the postfix expression that ends at t[end], or -1. */
    private static int postfixStart(List<ClangToken> t, int end) {
        int j = end;
        while (j >= 0) {
            String s = t.get(j).getText();
            if (s.equals("]") || s.equals(")")) {
                String open = s.equals("]") ? "[" : "(";
                int depth = 0;
                for (; j >= 0; j--) {
                    String u = t.get(j).getText();
                    if (u.equals(s)) depth++;
                    else if (u.equals(open) && --depth == 0) break;
                }
                if (j < 0) return -1;
                if (open.equals("(")) {
                    // A parenthesized primary ends the chain, unless a name before it calls it.
                    int p = prevTok(t, j);
                    if (p < 0 || !(t.get(p) instanceof ClangFuncNameToken)) return j;
                    return p;
                }
                j = prevTok(t, j);
                continue;
            }
            // A name: the chain goes on when "." or "->" is before it.
            int p = prevTok(t, j);
            if (p >= 0 && (t.get(p).getText().equals(".") || t.get(p).getText().equals("->"))) {
                j = prevTok(t, p);
                continue;
            }
            return j;
        }
        return -1;
    }

    /* The C type of the piece "_N_len_" whose "." is t[dot]: float or double when the p-code
     * piece is a float, else an unsigned int of len bytes. */
    private String pieceType(List<ClangToken> t, int dot, int len) {
        String type = len == 1 ? "uchar" : len == 2 ? "ushort" : len == 4 ? "uint" : len == 8 ? "ulonglong" : "uchar";
        PcodeOp op = t.get(dot).getPcodeOp();
        Varnode piece = null;
        if (op != null) {
            if (op.getOpcode() == PcodeOp.SUBPIECE) {
                piece = op.getOutput();
            } else {
                for (int k = 0; k < op.getNumInputs(); k++) {
                    PcodeOp d = op.getInput(k).getDef();
                    if (d != null && d.getOpcode() == PcodeOp.SUBPIECE) piece = op.getInput(k);
                }
            }
        }
        if (piece != null && piece.getSize() == len && piece.getHigh() != null
                && isFloat(piece.getHigh().getDataType(), len))
            type = len == 4 ? "float" : "double";
        // A write: "local_48._0_4_ = fVar5;" is a COPY whose output is the float piece. Not a
        // CAST: in "(float)x._188_4_" the CAST output is the float and the piece keeps the bits
        // of an int (setupobjectypes_train read it as a float and converted it to uint).
        if (piece == null && op != null && op.getOpcode() != PcodeOp.CAST
                && op.getOutput() != null && op.getOutput().getSize() == len
                && op.getOutput().getHigh() != null && isFloat(op.getOutput().getHigh().getDataType(), len))
            type = len == 4 ? "float" : "double";
        // A read by a float operation: "(double)local_60._0_4_" is FLOAT2FLOAT of the piece, and
        // "fVar6 < local_40._0_4_" is FLOAT_LESS (building_buildfloorplan). The piece has no
        // SUBPIECE: the stack piece is its own varnode.
        if (piece == null && op != null && (len == 4 || len == 8) && isFloatOp(op.getOpcode())) {
            for (int k = 0; k < op.getNumInputs(); k++)
                if (op.getInput(k).getSize() == len)
                    type = len == 4 ? "float" : "double";
        }
        return type;
    }

    /* A variable token whose high variable is smaller than its declared symbol: Ghidra gives
     * the name of an 8-byte stack variable (undefined8 local_48) to a float that lives in its
     * low 4 bytes. In C the name means the 8 bytes. Print the piece: the type of the high
     * variable at its offset in the symbol (packobjectpacket). */
    private String pieceVariable(List<ClangToken> t, int i) {
        if (!(t.get(i) instanceof ClangVariableToken)) return null;
        HighVariable h = t.get(i).getHighVariable();
        if (h == null || h.getSymbol() == null || h.getSymbol().getDataType() == null) return null;
        int whole = h.getSymbol().getDataType().getLength(), size = h.getDataType().getLength();
        // The same size is a piece too when the high variable is a float: "undefined4 local_14"
        // holds a float, and a read of local_14 in C converts the int (building_buildfloorplan).
        boolean sameFloat = size == whole && isFloat(h.getDataType(), size);
        if (size <= 0 || (size >= whole && !sameFloat) || !t.get(i).getText().equals(h.getSymbol().getName()))
            return null;
        // Only raw bytes (undefined8): an array such as float local_34[3] is not a piece.
        if (!(h.getSymbol().getDataType() instanceof Undefined)) return null;
        // Not in a declaration or in the function header.
        for (ClangNode p = t.get(i).Parent(); p != null; p = p.Parent())
            if (p instanceof ghidra.app.decompiler.ClangVariableDecl || p instanceof ghidra.app.decompiler.ClangFuncProto)
                return null;
        Varnode rep = h.getRepresentative();
        ghidra.program.model.listing.VariableStorage st = h.getSymbol().getStorage();
        if (rep == null || st == null || st.getMinAddress() == null || !rep.getAddress().isStackAddress()
                || !st.getMinAddress().isStackAddress()) return null;
        long off = rep.getOffset() - st.getMinAddress().getOffset();
        if (off < 0 || off + size > whole) return null;
        String type = isFloat(h.getDataType(), size) ? (size == 4 ? "float" : "double")
                : size == 1 ? "uchar" : size == 2 ? "ushort" : size == 4 ? "uint" : null;
        if (type == null) return null;
        return "(*(" + type + " *)((uchar *)&" + t.get(i).getText() + " + " + off + "))";
    }

    /* CONCAT44(hi, lo) is a PIECE of two 4-byte values. A float argument gives its bits, but
     * the C macro converts the number. Wrap it: LF_BITS_U(arg) (packobjectpacket). */
    private void pieceArgBits(List<ClangToken> t, Map<Integer, String> openBefore,
            Map<Integer, Integer> closeAfter) {
        for (int i = 0; i < t.size(); i++) {
            ClangToken tok = t.get(i);
            if (!tok.getText().startsWith("CONCAT")) continue;
            PcodeOp op = tok.getPcodeOp();
            if (op == null || op.getOpcode() != PcodeOp.PIECE || op.getNumInputs() != 2) continue;
            int open = next(t, i);
            if (open < 0 || !t.get(open).getText().equals("(")) continue;
            List<int[]> args = new ArrayList<>();
            int depth = 0, start = -1, j;
            for (j = open; j < t.size(); j++) {
                String s = t.get(j).getText();
                if (s.equals("(")) {
                    if (depth++ == 0) start = j + 1;
                } else if (s.equals(")")) {
                    if (--depth == 0) break;
                } else if (s.equals(",") && depth == 1) {
                    args.add(new int[] { start, j - 1 });
                    start = j + 1;
                }
            }
            if (j >= t.size()) continue;
            args.add(new int[] { start, j - 1 });
            if (args.size() != 2) continue;
            for (int k = 0; k < 2; k++) {
                Varnode v = op.getInput(k);
                if (v.getHigh() == null || !isFloat(v.getHigh().getDataType(), v.getSize())) continue;
                int first = args.get(k)[0], last = args.get(k)[1];
                while (first <= last && t.get(first).getText().isBlank()) first++;
                while (last >= first && t.get(last).getText().isBlank()) last--;
                if (first > last) continue;
                openBefore.merge(first, v.getSize() == 4 ? "LF_BITS_U(" : "LF_BITS_U64(", (x, y) -> y + x);
                closeAfter.merge(last, 1, Integer::sum);
            }
        }
    }

    private static final Set<Integer> FLOAT_OPS = Set.of(
            PcodeOp.FLOAT_ADD, PcodeOp.FLOAT_SUB, PcodeOp.FLOAT_MULT, PcodeOp.FLOAT_DIV,
            PcodeOp.FLOAT_NEG, PcodeOp.FLOAT_ABS, PcodeOp.FLOAT_SQRT, PcodeOp.FLOAT_INT2FLOAT,
            PcodeOp.FLOAT_FLOAT2FLOAT, PcodeOp.FLOAT_CEIL, PcodeOp.FLOAT_FLOOR, PcodeOp.FLOAT_ROUND);

    /* A p-code operation whose inputs are all floats (INT2FLOAT is not: its input is an int). */
    private static boolean isFloatOp(int code) {
        return code != PcodeOp.FLOAT_INT2FLOAT && (FLOAT_OPS.contains(code) || code == PcodeOp.FLOAT_LESS
                || code == PcodeOp.FLOAT_LESSEQUAL || code == PcodeOp.FLOAT_EQUAL || code == PcodeOp.FLOAT_NOTEQUAL
                || code == PcodeOp.FLOAT_TRUNC || code == PcodeOp.FLOAT_NAN);
    }

    private static boolean isFloat(DataType d, int len) {
        return d != null && d.getLength() == len && d.getName().equals(len == 4 ? "float" : "double");
    }

    /* A float stored into raw bytes (undefined4 or undefined8). Ghidra prints "X = f;" and
     * means the bits of f. In C the assignment converts the number (0.42 becomes 0). Mark
     * the left side: "*(float *)&(X) = f;" (double for 8 bytes). The p-code gives the types,
     * so "<=" in a condition or a statement on many lines does not matter. */
    private void floatStores(List<ClangToken> t, Map<Integer, String> openBefore,
            Map<Integer, Integer> closeAfter) {
        Map<ClangToken, Integer> index = new IdentityHashMap<>();
        for (int i = 0; i < t.size(); i++) index.put(t.get(i), i);
        for (int i = 0; i < t.size(); i++) {
            ClangToken tok = t.get(i);
            if (!(tok instanceof ClangOpToken) || !tok.getText().equals("=")) continue;
            PcodeOp op = tok.getPcodeOp();
            if (op == null) continue;
            DataType dst, val;
            boolean floatValue;
            if (op.getOpcode() == PcodeOp.STORE) {
                if (op.getNumInputs() < 3 || op.getInput(1).getHigh() == null || op.getInput(2).getHigh() == null)
                    continue;
                DataType p = op.getInput(1).getHigh().getDataType();
                if (!(p instanceof Pointer)) continue;
                dst = ((Pointer) p).getDataType();
                val = op.getInput(2).getHigh().getDataType();
                floatValue = dst != null && isFloat(val, dst.getLength());
            } else {
                // A variable: the C declaration has the symbol type (undefined8 local_30), but
                // the decompiler can use the variable as another type (double).
                HighVariable h = op.getOutput() == null ? null : op.getOutput().getHigh();
                if (h == null) continue;
                HighSymbol sym = h.getSymbol();
                dst = sym != null && sym.getDataType() != null ? sym.getDataType() : h.getDataType();
                int n = dst.getLength();
                floatValue = isFloat(h.getDataType(), n)
                        || FLOAT_OPS.contains(op.getOpcode()) && op.getOutput().getSize() == n
                        || op.getOpcode() == PcodeOp.COPY && op.getInput(0).getHigh() != null
                                && isFloat(op.getInput(0).getHigh().getDataType(), n);
            }
            if (dst == null || !(dst instanceof Undefined) || !floatValue) continue;
            int len = dst.getLength();
            if (len != 4 && len != 8) continue;
            // "X = 0x3f800000;" already gives the bits.
            int rhs = next(t, i);
            if (rhs >= 0 && t.get(rhs).getText().matches("-?0x[0-9a-fA-F]+")) {
                int after = next(t, rhs);
                if (after >= 0 && t.get(after).getText().equals(";")) continue;
            }
            // The left side: from the first token of the statement to the token before "=".
            ClangNode s = tok.Parent();
            while (s != null && !(s instanceof ClangStatement)) s = s.Parent();
            if (s == null) continue;
            List<ClangNode> leaves = new ArrayList<>();
            s.flatten(leaves);
            Integer first = null;
            for (ClangNode n : leaves) {
                if (!((ClangToken) n).getText().isBlank()) {
                    first = index.get((ClangToken) n);
                    break;
                }
            }
            int last = i - 1;
            while (last >= 0 && t.get(last).getText().isBlank()) last--;
            if (first == null || first > last) continue;
            String head = t.get(first).getText();
            if (head.equals("for") || head.equals("if") || head.equals("while") || head.equals("return")) continue;
            openBefore.merge(first, len == 4 ? "*(float *)&(" : "*(double *)&(", (a, b) -> b + a);
            closeAfter.merge(last, 1, Integer::sum);
        }
    }

    /* A call argument printed with the caller type. The original pushes the bits of the
     * argument unchanged, but in C the call converts the number to the callee parameter type:
     *   float parameter, pointer argument      -> lf_p2f(arg)
     *   float parameter, other 4-byte argument -> lf_u2f(arg)   (0x3f8c0000 is 1.09375)
     *   int parameter, float argument          -> lf_f2u(arg)
     * The argument type comes from the p-code input of the CALL, the parameter type from the
     * callee signature. Only calls to functions of the program (not imports). */
    private void callArgBits(List<ClangToken> t, Map<Integer, String> openBefore,
            Map<Integer, Integer> closeAfter) {
        for (int i = 0; i < t.size(); i++) {
            if (!(t.get(i) instanceof ClangFuncNameToken)) continue;
            PcodeOp op = t.get(i).getPcodeOp();
            if (op == null || op.getOpcode() != PcodeOp.CALL) continue;
            Function callee = getFunctionAt(op.getInput(0).getAddress());
            if (callee == null || callee.isExternal() || callee.isThunk()) continue;
            DataType[] ps = paramTypes(callee);
            int open = next(t, i);
            if (open < 0 || !t.get(open).getText().equals("(")) continue;
            // Split the arguments at the commas of depth 1.
            List<int[]> args = new ArrayList<>();
            int depth = 0, start = -1, j;
            for (j = open; j < t.size(); j++) {
                String s = t.get(j).getText();
                if (s.equals("(")) {
                    if (depth++ == 0) start = j + 1;
                } else if (s.equals(")")) {
                    if (--depth == 0) break;
                } else if (s.equals(",") && depth == 1) {
                    args.add(new int[] { start, j - 1 });
                    start = j + 1;
                }
            }
            if (j >= t.size()) continue;
            if (start <= j - 1) args.add(new int[] { start, j - 1 });
            if (args.size() != op.getNumInputs() - 1) continue;
            for (int k = 0; k < args.size() && k < ps.length; k++) {
                Varnode v = op.getInput(k + 1);
                if (v.getSize() != 4 || v.getHigh() == null) continue;
                DataType a = v.getHigh().getDataType();
                DataType p = ps[k];
                if (p == null || p.getLength() != 4) continue;
                int first = args.get(k)[0], last = args.get(k)[1];
                while (first <= last && t.get(first).getText().isBlank()) first++;
                while (last >= first && t.get(last).getText().isBlank()) last--;
                if (first > last) continue;
                String wrap = null;
                int close = 1;
                if (isFloat(p, 4) && !isFloat(a, 4)) {
                    if (a instanceof Pointer) wrap = "lf_p2f(";
                    else if (v.isConstant()) wrap = "lf_u2f(";
                    else {
                        wrap = "lf_u2f((uint)(";
                        close = 2;
                    }
                } else if (!isFloat(p, 4) && !(p instanceof Pointer) && isFloat(a, 4)) {
                    wrap = "lf_f2u(";
                }
                if (wrap == null) continue;
                // "0" is 0.0 as a float too.
                if (v.isConstant() && v.getOffset() == 0) continue;
                openBefore.merge(first, wrap, (x, y) -> y + x);
                closeAfter.merge(last, close, Integer::sum);
            }
        }
    }

    /* The original keeps x87 values in registers with 53 bits, but it rounds a value to 24 bits
     * when it stores it as a float (fstp dword ptr [esp+10h]) and reads it back. The p-code
     * of the decompiler has only 4-byte float ops, so it cannot show which results the
     * original rounded. Look at the machine code: if the result of a float op goes to a
     * dword store before any other x87 op uses it, print "(float)(...)" around the op.
     * human_calcarmtorque summed four terms with a store after each add. */
    private void storeRounding(List<ClangToken> t, Map<Integer, String> openBefore,
            Map<Integer, Integer> closeAfter) {
        Set<PcodeOp> done = new HashSet<>();
        for (int i = 0; i < t.size(); i++) {
            ClangToken tok = t.get(i);
            if (!(tok instanceof ClangOpToken)) continue;
            PcodeOp op = tok.getPcodeOp();
            if (op == null || done.contains(op) || op.getOutput() == null || op.getOutput().getSize() != 4) continue;
            int code = op.getOpcode();
            if (code != PcodeOp.FLOAT_ADD && code != PcodeOp.FLOAT_SUB && code != PcodeOp.FLOAT_MULT
                    && code != PcodeOp.FLOAT_DIV) continue;
            String text = tok.getText();
            if (!(text.equals("+") || text.equals("-") || text.equals("*") || text.equals("/"))) continue;
            done.add(op);
            if (!storedAsFloat(op.getSeqnum().getTarget())) continue;
            Set<PcodeOp> self = subtree(op.getOutput());
            int start = i, depth = 0;
            for (int j = i - 1; j >= 0; j--) {
                ClangToken u = t.get(j);
                if (u instanceof ClangBreak || u.getText().isBlank()) continue;
                String x = u.getText();
                if (x.equals(")")) {
                    depth++;
                } else if (x.equals("(")) {
                    if (depth == 0) break;
                    depth--;
                } else if (depth == 0 && (x.equals("=") || x.equals(",") || x.equals(";") || x.equals("{")
                        || x.equals("}") || x.equals("return"))) {
                    break;
                } else if (depth == 0 && u.getPcodeOp() != null && !self.contains(u.getPcodeOp())) {
                    break;
                }
                start = j;
            }
            if (depth != 0) continue;
            int end = -1;
            for (int j = nextAcross(t, i); j >= 0 && j < t.size(); j++) {
                ClangToken u = t.get(j);
                if (u instanceof ClangBreak || u.getText().isBlank()) continue;
                String x = u.getText();
                if (x.equals("(")) {
                    depth++;
                } else if (x.equals(")")) {
                    if (depth == 0) break;
                    depth--;
                } else if (depth == 0 && (x.equals(";") || x.equals(","))) {
                    break;
                } else if (depth == 0 && u.getPcodeOp() != null && !self.contains(u.getPcodeOp())) {
                    break;
                }
                end = j;
            }
            if (end < 0 || depth != 0 || start == i) continue;
            openBefore.merge(start, "(float)(", (a, b) -> b + a);
            closeAfter.merge(end, 1, Integer::sum);
        }
    }

    /* Follow the x87 stack from the instruction at a: true when its result is stored as a
     * dword (rounded to float) before another x87 instruction reads it. */
    private boolean storedAsFloat(Address a) {
        Listing listing = currentProgram.getListing();
        Instruction in = listing.getInstructionAt(a);
        if (in == null) return false;
        String m = in.getMnemonicString().toUpperCase();
        // SSE (mulps, addps, dpps...) rounds every float op to 24 bits. bondsimulation2 has
        // a block of SSE4.1 code.
        for (int o = 0; o < in.getNumOperands(); o++)
            if (in.getDefaultOperandRepresentation(o).toUpperCase().contains("XMM")) return true;
        if (!m.startsWith("F")) return false;
        // Where the result is: ST0, or ST(i) for "fop st(i),st" (minus one for a pop).
        int r = 0;
        if (in.getNumOperands() == 2) {
            int d = stIndex(in.getDefaultOperandRepresentation(0));
            if (d > 0) r = m.endsWith("P") ? d - 1 : d;
        }
        for (int n = 0; n < 16; n++) {
            in = in.getNext();
            if (in == null) return false;
            m = in.getMnemonicString().toUpperCase();
            if (in.getFlowType().isCall() || in.getFlowType().isJump() || in.getFlowType().isTerminal()) return false;
            if (!m.startsWith("F")) continue;
            String o0 = in.getNumOperands() > 0 ? in.getDefaultOperandRepresentation(0) : "";
            String o1 = in.getNumOperands() > 1 ? in.getDefaultOperandRepresentation(1) : "";
            if (m.equals("FST") || m.equals("FSTP")) {
                if (r == 0) return o0.toLowerCase().startsWith("float ptr") || o0.toLowerCase().startsWith("dword ptr");
                if (m.equals("FSTP")) r--;
                continue;
            }
            if (m.startsWith("FLD") || m.equals("FILD")) {
                r++;
                continue;
            }
            if (m.equals("FXCH")) {
                int x = o0.isEmpty() ? 1 : stIndex(o0);
                if (r == 0) r = x;
                else if (r == x) r = 0;
                continue;
            }
            // Any other x87 instruction that touches the value uses it unrounded.
            int s0 = stIndex(o0), s1 = stIndex(o1);
            if (in.getNumOperands() == 0 && m.endsWith("P")) {
                s0 = 1;  // "faddp" alone is "faddp st(1),st"
                s1 = 0;
            }
            if (r == 0 || s0 == r || s1 == r) return false;
            if (m.endsWith("P")) r--;
            if (r < 0) return false;
        }
        return false;
    }

    /* 2 for "ST(2)" or "ST2", 0 for "ST" or "ST0", -1 when it is not an x87 register. */
    private static int stIndex(String o) {
        String x = o.toUpperCase().replace(" ", "");
        if (x.equals("ST") || x.equals("ST0") || x.equals("ST(0)")) return 0;
        java.util.regex.Matcher mm = java.util.regex.Pattern.compile("^ST\\(?(\\d)\\)?$").matcher(x);
        return mm.find() ? Integer.parseInt(mm.group(1)) : -1;
    }

    /* Float math is not associative: (a + b) + c and a + (b + c) round differently. Ghidra
     * prints the p-code tree FLOAT_ADD(x, FLOAT_ADD(y, z)) as "x + y + z", which C reads as
     * (x + y) + z. The same for * and /. Put the parentheses back around a right operand
     * that is itself a + or - (for + and -) or a * or / (for * and /).
     * human_calcarmtorque summed three squares in another order. The 1-ulp difference went
     * through acos() near 1 and became 1.6e-5. */
    private void floatGrouping(List<ClangToken> t, Map<Integer, String> openBefore,
            Map<Integer, Integer> closeAfter) {
        for (int i = 0; i < t.size(); i++) {
            ClangToken tok = t.get(i);
            if (!(tok instanceof ClangOpToken)) continue;
            PcodeOp op = tok.getPcodeOp();
            if (op == null || op.getNumInputs() != 2) continue;
            int code = op.getOpcode();
            Set<Integer> group;
            if (code == PcodeOp.FLOAT_ADD || code == PcodeOp.FLOAT_SUB)
                group = Set.of(PcodeOp.FLOAT_ADD, PcodeOp.FLOAT_SUB);
            else if (code == PcodeOp.FLOAT_MULT || code == PcodeOp.FLOAT_DIV)
                group = Set.of(PcodeOp.FLOAT_MULT, PcodeOp.FLOAT_DIV);
            else
                continue;
            String text = tok.getText();
            if (!(text.equals("+") || text.equals("-") || text.equals("*") || text.equals("/"))) continue;
            int start = nextAcross(t, i);
            if (start < 0) continue;
            // Which input is printed on the right: the first token with an op tells.
            Set<PcodeOp> s0 = subtree(op.getInput(0)), s1 = subtree(op.getInput(1));
            int probe = start;
            while (probe >= 0 && t.get(probe).getPcodeOp() == null) probe = nextAcross(t, probe);
            if (probe < 0) continue;
            PcodeOp first = t.get(probe).getPcodeOp();
            Set<PcodeOp> right;
            Varnode rv;
            if (s1.contains(first)) {
                right = s1;
                rv = op.getInput(1);
            } else if (s0.contains(first)) {
                right = s0;
                rv = op.getInput(0);
            } else {
                continue;  // a leaf: a variable or a constant
            }
            PcodeOp def = rv.getDef();
            if (def == null || !group.contains(def.getOpcode())) continue;
            // The end of the right operand: the last token before a token of another op at
            // depth 0, or before ";", "," or an unmatched ")".
            int depth = 0, end = -1;
            for (int j = start; j < t.size(); j++) {
                ClangToken u = t.get(j);
                if (u instanceof ClangBreak || u.getText().isBlank()) continue;
                String x = u.getText();
                if (x.equals("(")) {
                    depth++;
                } else if (x.equals(")")) {
                    if (depth == 0) break;
                    depth--;
                } else if (depth == 0 && (x.equals(";") || x.equals(","))) {
                    break;
                } else if (depth == 0 && u.getPcodeOp() != null && !right.contains(u.getPcodeOp())) {
                    break;
                }
                end = j;
            }
            if (end < 0 || depth != 0) continue;
            if (t.get(start).getText().equals("(") && matching(t, start) == end) continue;
            openBefore.merge(start, "(", (a, b) -> b + a);
            closeAfter.merge(end, 1, Integer::sum);
        }
    }

    /* The ops of the expression that computes v, down to named variables. */
    private static Set<PcodeOp> subtree(Varnode v) {
        Set<PcodeOp> out = new HashSet<>();
        Deque<Varnode> todo = new ArrayDeque<>();
        todo.push(v);
        while (!todo.isEmpty() && out.size() < 512) {
            Varnode x = todo.pop();
            PcodeOp d = x.getDef();
            if (d == null || out.contains(d)) continue;
            int c = d.getOpcode();
            if (c == PcodeOp.MULTIEQUAL || c == PcodeOp.INDIRECT) continue;
            if (x != v && x.getHigh() != null && x.getHigh().getSymbol() != null) continue;
            out.add(d);
            for (int k = 0; k < d.getNumInputs(); k++) todo.push(d.getInput(k));
        }
        return out;
    }

    /* The next token that is not blank, on this line or a later one. */
    private static int nextAcross(List<ClangToken> t, int i) {
        for (int j = i + 1; j < t.size(); j++) {
            if (t.get(j) instanceof ClangBreak) continue;
            if (!t.get(j).getText().isBlank()) return j;
        }
        return -1;
    }

    private static int matching(List<ClangToken> t, int open) {
        int depth = 0;
        for (int j = open; j < t.size(); j++) {
            String x = t.get(j).getText();
            if (x.equals("(")) depth++;
            else if (x.equals(")") && --depth == 0) return j;
        }
        return -1;
    }

    private final Map<Integer, Object[]> replaceSpans = new HashMap<>();

    /* An integer compare (INT_EQUAL, INT_NOTEQUAL) of a float-typed value compares bits. Ghidra
     * prints it as a float compare: "fVar5 != -NAN" for "parent index != -1". In C every
     * compare with a NaN is unordered, so "!=" is always true. Print the float side as
     * LF_BITS_U(x) and a float constant as its bits. itemsimulation copied item[-1]. */
    private static final java.util.regex.Pattern NUMBER = java.util.regex.Pattern.compile("^(-?)(0x[0-9a-fA-F]+|\\d+)$");

    /* Absolute addresses that the loader relocates, printed as numbers. The lifted code runs
     * the 024d at another base, so each one becomes SR_VA() of the original address.
     *
     * A constant token is relocated when the instruction of its p-code op has a relocation
     * whose value is the constant, or the constant minus a small offset (Ghidra adds the
     * offset of a field: "iVar2 + 0x9fdd8a8" for &DAT_09fdd8a4 + 4). The index of a PTRADD
     * is relocated when index * element size is such a value (a pointer that counts from 0,
     * "local_cc[0x1664980]" in humansimulation). Without a relocation in the instruction,
     * an exact relocated value of the function counts (Ghidra can move a constant away from
     * its instruction). A RAM address without a symbol ("ram0x1992d6ed") becomes a pointer
     * access to SR_VA(). Each token is changed once, so no address gets two relocations. */
    private void relocatedConstants(List<ClangToken> t, Function f) throws Exception {
        Map<Address, Set<Long>> byIns = new HashMap<>();
        Set<Long> all = new HashSet<>();
        Iterator<Relocation> it = currentProgram.getRelocationTable().getRelocations(f.getBody());
        while (it.hasNext()) {
            Relocation rel = it.next();
            long v = currentProgram.getMemory().getInt(rel.getAddress()) & 0xffffffffL;
            Instruction ins = currentProgram.getListing().getInstructionContaining(rel.getAddress());
            if (ins != null) byIns.computeIfAbsent(ins.getAddress(), k -> new HashSet<>()).add(v);
            all.add(v);
        }
        if (all.isEmpty()) return;
        for (int i = 0; i < t.size(); i++) {
            if (replaceSpans.containsKey(i) || !(t.get(i) instanceof ClangVariableToken)) continue;
            ClangToken tok = t.get(i);
            String text = tok.getText();
            Varnode vn = ((ClangVariableToken) tok).getVarnode();
            if (text.startsWith("ram0x") && vn != null && vn.isAddress()) {
                long a = vn.getAddress().getOffset();
                String ty = vn.getSize() == 1 ? "uchar" : vn.getSize() == 2 ? "ushort" : vn.getSize() == 8 ? "undefined8" : "undefined4";
                replaceSpans.put(i, new Object[] { i, String.format("(*(%s *)SR_VA(0x%x))", ty, a) });
                continue;
            }
            java.util.regex.Matcher m = NUMBER.matcher(text);
            if (!m.find()) continue;
            boolean neg = !m.group(1).isEmpty();
            String digits = m.group(2);
            // A 64-bit constant (a longlong mask) is never a 32-bit address.
            if (digits.length() > 10) continue;
            long val = digits.startsWith("0x") ? Long.parseLong(digits.substring(2), 16) : Long.parseLong(digits);
            if (val > 0xffffffffL) continue;
            PcodeOp op = tok.getPcodeOp();
            Set<Long> local = Collections.emptySet();
            if (op != null) {
                Instruction ins = currentProgram.getListing().getInstructionContaining(op.getSeqnum().getTarget());
                if (ins != null) local = byIns.getOrDefault(ins.getAddress(), Collections.emptySet());
            }
            // The index of a PTRADD: index * element size is the address.
            if (op != null && op.getOpcode() == PcodeOp.PTRADD && !neg && op.getNumInputs() == 3
                    && op.getInput(1).isConstant() && op.getInput(1).getOffset() == val && op.getInput(2).isConstant()) {
                long size = op.getInput(2).getOffset();
                long addr = val * size;
                if (size > 0 && (local.contains(addr) || all.contains(addr))) {
                    replaceSpans.put(i, new Object[] { i, String.format("(0x%x + LF_RELOC / %d)", val, size) });
                    continue;
                }
            }
            if (val < 0x10000) continue;
            String rel = relocText(val, local, all);
            if (rel != null)
                replaceSpans.put(i, new Object[] { i, neg ? "(-" + rel + ")" : rel });
        }
    }

    /* "((int)SR_VA(0xV))" or "((int)SR_VA(0xV) + d)" for a value V that the loader relocates. */
    private static String relocText(long val, Set<Long> local, Set<Long> all) {
        if (local.contains(val) || all.contains(val))
            return String.format("((int)SR_VA(0x%x))", val);
        // A field offset inside the same instruction, or "END - 1" in a compare.
        for (long r : local) {
            long d = val - r;
            if (d >= -0x10000 && d <= 0x10000 && d != 0)
                return String.format("((int)SR_VA(0x%x) %s %d)", r, d < 0 ? "-" : "+", Math.abs(d));
        }
        // Ghidra folded the field offset into a constant and moved it to another instruction:
        // "iVar2 + 0x9fdd8a8" for the relocated 0x9fdd8a4 plus 4 (newspaper_load). Only when no
        // relocation of this instruction explains the value.
        if (local.isEmpty()) {
            long best = -1;
            for (long r : all)
                if (val > r && val - r <= 0x40 && (best < 0 || val - r < val - best)) best = r;
            if (best >= 0)
                return String.format("((int)SR_VA(0x%x) + %d)", best, val - best);
        }
        if (all.contains(val + 1))
            return String.format("((int)SR_VA(0x%x) - 1)", val + 1);
        if (all.contains(val - 1))
            return String.format("((int)SR_VA(0x%x) + 1)", val - 1);
        return null;
    }

    private void floatBitCompares(List<ClangToken> t, Map<Integer, String> openBefore,
            Map<Integer, Integer> closeAfter) {
        for (int i = 0; i < t.size(); i++) {
            ClangToken tok = t.get(i);
            if (!(tok instanceof ClangOpToken)) continue;
            PcodeOp op = tok.getPcodeOp();
            if (op == null || op.getNumInputs() != 2) continue;
            if (op.getOpcode() != PcodeOp.INT_EQUAL && op.getOpcode() != PcodeOp.INT_NOTEQUAL) continue;
            if (!tok.getText().equals("==") && !tok.getText().equals("!=")) continue;
            Varnode a = op.getInput(0), b = op.getInput(1);
            int len = a.getSize();
            if (len != b.getSize() || (len != 4 && len != 8)) continue;
            boolean af = a.getHigh() != null && isFloat(a.getHigh().getDataType(), len);
            boolean bf = b.getHigh() != null && isFloat(b.getHigh().getDataType(), len);
            if (!af && !bf) continue;
            // The two sides of the operator, from the p-code of each input.
            int[] left = side(t, i, op, -1), right = side(t, i, op, 1);
            if (left == null || right == null) continue;
            for (int[] sd : new int[][] { left, right }) {
                Varnode v = sd[2] == 0 ? a : b;
                if (v.isConstant()) {
                    if (!(sd[2] == 0 ? af : bf) && !(sd[2] == 0 ? bf : af)) continue;
                    String bits = len == 4 ? String.format("0x%xU", v.getOffset() & 0xffffffffL)
                            : String.format("0x%xULL", v.getOffset());
                    replaceSpans.put(sd[0], new Object[] { sd[1], bits });
                } else if (sd[2] == 0 ? af : bf) {
                    openBefore.merge(sd[0], len == 4 ? "LF_BITS_U(" : "LF_BITS_U64(", (x, y) -> y + x);
                    closeAfter.merge(sd[1], 1, Integer::sum);
                }
            }
        }
    }

    /* The token span {start, end, input index} of one side of the binary op at t[i]
     * (dir -1: left, 1: right), or null. */
    private int[] side(List<ClangToken> t, int i, PcodeOp op, int dir) {
        Set<PcodeOp> s0 = subtree(op.getInput(0)), s1 = subtree(op.getInput(1));
        int depth = 0, first = -1, last = -1, which = -1;
        for (int j = i + dir; j >= 0 && j < t.size(); j += dir) {
            ClangToken u = t.get(j);
            if (u instanceof ClangBreak || u.getText().isBlank()) continue;
            String x = u.getText();
            String open = dir > 0 ? "(" : ")", close = dir > 0 ? ")" : "(";
            if (x.equals(open) || (dir > 0 ? x.equals("[") : x.equals("]"))) {
                depth++;
            } else if (x.equals(close) || (dir > 0 ? x.equals("]") : x.equals("["))) {
                if (depth == 0) break;
                depth--;
            } else if (depth == 0 && (x.equals(";") || x.equals(",") || x.equals("=") || x.equals("&&")
                    || x.equals("||") || x.equals("return"))) {
                break;
            } else if (depth == 0 && u.getPcodeOp() != null && u.getPcodeOp() != op
                    && !s0.contains(u.getPcodeOp()) && !s1.contains(u.getPcodeOp())) {
                break;
            }
            PcodeOp p = u.getPcodeOp();
            if (which < 0 && p != null) {
                if (s0.contains(p)) which = 0;
                else if (s1.contains(p)) which = 1;
                else if (p == op && u.getHighVariable() != null) {
                    if (u.getHighVariable() == op.getInput(0).getHigh()) which = 0;
                    else if (u.getHighVariable() == op.getInput(1).getHigh()) which = 1;
                }
            }
            if (first < 0) first = j;
            last = j;
        }
        if (first < 0 || depth != 0 || which < 0) return null;
        return dir > 0 ? new int[] { first, last, which } : new int[] { last, first, which };
    }

    private final Map<Address, DataType[]> protos = new HashMap<>();
    private DecompInterface protoDi;

    /* The parameter types of a function as the decompiler shows them (the pseudo-C header that
     * lift.py reads). The stored signature can have no parameters at all. */
    private DataType[] paramTypes(Function f) {
        DataType[] r = protos.get(f.getEntryPoint());
        if (r != null) return r;
        r = new DataType[0];
        if (protoDi == null) {
            protoDi = new DecompInterface();
            protoDi.setOptions(new DecompileOptions());
            protoDi.openProgram(currentProgram);
        }
        DecompileResults d = protoDi.decompileFunction(f, 120, monitor);
        HighFunction h = d.decompileCompleted() ? d.getHighFunction() : null;
        if (h != null) {
            ghidra.program.model.pcode.FunctionPrototype fp = h.getFunctionPrototype();
            r = new DataType[fp.getNumParams()];
            for (int i = 0; i < r.length; i++) r[i] = fp.getParam(i).getDataType();
        }
        protos.put(f.getEntryPoint(), r);
        return r;
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

    private int[] bitCastSimple(List<ClangToken> t, int i) {
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
            end = postfixEnd(t, k);
            // "(float)g_body[1].unk_0010": the cast reads the whole postfix chain. The CAST
            // consumes the output of an op in the chain (the field load).
            if (end != k) {
                castOp = null;
                chain:
                for (int j = k; j <= end; j++) {
                    PcodeOp p = t.get(j).getPcodeOp();
                    if (p == null || p.getOutput() == null) continue;
                    Iterator<PcodeOp> d = p.getOutput().getDescendants();
                    while (d.hasNext()) {
                        PcodeOp q = d.next();
                        if (q.getOpcode() == PcodeOp.CAST) {
                            castOp = q;
                            break chain;
                        }
                    }
                }
            }
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
        int kind = castKind(castOp);
        if (kind < 0 || (kind % 2 == 1 && integerCastAt(t, k))) return null;
        // The CAST must be the printed cast: in "(uint)(float)local_48" the CAST is the
        // "(uint)", and "(float)" is the SUBPIECE of an 8-byte variable.
        if (!castType(t, i, close).replace(" ", "")
                .equals(castOp.getOutput().getHigh().getDataType().getName().replace(" ", ""))) return null;
        return new int[] { close, end, kind };
    }

    /* A cast from a float whose operand starts with a cast to an integer type:
     * "(int)(longlong)(x * f)". In C the
     * operand is then an integer, and "(int)" keeps its low bits like the p-code SUBPIECE.
     * Ghidra types the 4-byte piece of the stack slot as a float (the slot also holds a
     * float), so the CAST looks like a bit cast. LF_BITS_U would convert the longlong to a
     * float (logic_addteamcash added the bits of 49.1 to the player money). */
    private static boolean integerCastAt(List<ClangToken> t, int k) {
        if (!t.get(k).getText().equals("(")) return false;
        int ty = next(t, k);
        if (ty < 0 || !(t.get(ty) instanceof ClangTypeToken)) return false;
        int close = next(t, ty);
        if (close < 0 || !t.get(close).getText().equals(")")) return false;
        String name = t.get(ty).getText();
        return !name.equals("float") && !name.equals("double") && !name.equals("float10");
    }

    /* 0: 4 bytes to float, 1: float to 4 bytes, 2: 8 bytes to double, 3: double to 8 bytes,
     * -1: not a bit cast between a float and an int of the same size. */
    private static int castKind(PcodeOp castOp) {
        if (castOp == null || castOp.getOpcode() != PcodeOp.CAST) return -1;
        if (castOp.getOutput() == null || castOp.getOutput().getHigh() == null || castOp.getInput(0).getHigh() == null)
            return -1;
        DataType dst = castOp.getOutput().getHigh().getDataType();
        DataType src = castOp.getInput(0).getHigh().getDataType();
        int len = dst.getLength();
        if (len != src.getLength() || (len != 4 && len != 8)) return -1;
        boolean dstF = dst.getName().equals(len == 4 ? "float" : "double");
        boolean srcF = src.getName().equals(len == 4 ? "float" : "double");
        if (dstF == srcF) return -1;
        return (len == 4 ? 0 : 2) + (dstF ? 0 : 1);
    }

    /* A bit cast at t[i]: the simple forms "(T)var", "(T)(...)", "(T)F(...)", then any operand
     * such as "(int)*p" or "(int)p[2]". createobject read a body index with (int)*pfVar14 and
     * converted the float to an int. The end of the operand comes from the p-code: the tokens
     * of the expression that the CAST reads. */
    private int[] bitCast(List<ClangToken> t, int i) {
        int[] r = bitCastSimple(t, i);
        if (r != null) return r;
        if (!(t.get(i) instanceof ClangSyntaxToken) || !t.get(i).getText().equals("(")) return null;
        int ty = next(t, i);
        if (ty < 0 || !(t.get(ty) instanceof ClangTypeToken)) return null;
        int close = next(t, ty);
        while (close >= 0 && t.get(close).getText().equals("*")) close = next(t, close);
        if (close < 0 || !t.get(close).getText().equals(")")) return null;
        int k = nextAcross(t, close);
        if (k < 0) return null;
        String printed = castType(t, i, close).replace(" ", "");
        int u = k;
        while (u >= 0 && t.get(u).getPcodeOp() == null) u = nextAcross(t, u);
        if (u < 0) return null;
        // The CAST: the op of the first token, or an op that uses its result (a few levels).
        PcodeOp castOp = null;
        Deque<PcodeOp> todo = new ArrayDeque<>();
        todo.add(t.get(u).getPcodeOp());
        for (int n = 0; n < 16 && !todo.isEmpty() && castOp == null; n++) {
            PcodeOp p = todo.poll();
            if (p.getOpcode() == PcodeOp.CAST) {
                castOp = p;
                break;
            }
            int c = p.getOpcode();
            // A conversion op with the printed type is the printed cast: "(float)dVar10". One
            // with another type is inside the operand: "(float *)(float)dVar39".
            if ((c == PcodeOp.FLOAT_FLOAT2FLOAT || c == PcodeOp.FLOAT_INT2FLOAT || c == PcodeOp.FLOAT_TRUNC
                    || c == PcodeOp.INT_ZEXT || c == PcodeOp.INT_SEXT) && p.getOutput() != null
                    && p.getOutput().getHigh() != null
                    && printed.equals(p.getOutput().getHigh().getDataType().getName().replace(" ", ""))) break;
            if (p.getOutput() == null) continue;
            Iterator<PcodeOp> d = p.getOutput().getDescendants();
            while (d.hasNext()) todo.add(d.next());
        }
        if (castKind(castOp) < 0 || (castKind(castOp) % 2 == 1 && integerCastAt(t, k))) return null;
        // It must be the printed cast: "(float)dVar10" is a FLOAT2FLOAT, and a bit CAST of
        // its result further on is another op.
        if (!printed.equals(castOp.getOutput().getHigh().getDataType().getName().replace(" ", ""))) return null;
        Set<PcodeOp> allowed = subtree(castOp.getInput(0));
        allowed.add(castOp);
        if (!allowed.contains(t.get(u).getPcodeOp())) return null;
        int depth = 0, end = -1;
        for (int j = k; j >= 0 && j < t.size(); j++) {
            ClangToken x = t.get(j);
            if (x instanceof ClangBreak) continue;  // an operand on several lines
            String s = x.getText();
            if (s.isBlank()) continue;
            if (s.equals("(") || s.equals("[")) {
                depth++;
            } else if (s.equals(")") || s.equals("]")) {
                if (depth == 0) break;
                depth--;
            } else if (depth == 0 && (s.equals(";") || s.equals(","))) {
                break;
            } else if (depth == 0 && (s.equals("&&") || s.equals("||") || s.equals("?") || s.equals(":"))) {
                // An "&&" that joins the conditions of nested ifs has no p-code op. The operand
                // of a cast never holds one at depth 0. objectsimulation wrapped the next
                // condition in LF_BITS_F and never called lineintersectobject.
                break;
            } else if (depth == 0 && x.getPcodeOp() != null && !allowed.contains(x.getPcodeOp())) {
                break;
            }
            end = j;
        }
        if (end < k || depth != 0) return null;
        return new int[] { close, end, castKind(castOp) };
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
            // A named global (ApplyTypes.java) prints "_g_intersect" for a piece of g_intersect:
            // the decompiler marks it as a global variable.
            boolean global = hv instanceof ghidra.program.model.pcode.HighGlobal;
            if (!gm.matches() && !global) continue;
            // The real address: "DAT_107575c0_1" can be a piece at 0x107575c1.
            String hexAddr = gm.matches() ? gm.group(1) : null;
            ghidra.program.model.pcode.Varnode rep = hv.getRepresentative();
            if (rep != null && rep.getAddress().isMemoryAddress()) {
                hexAddr = String.format("%08x", rep.getAddress().getOffset());
            }
            if (hexAddr == null) continue;
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
