// Export the features of each function to a JSON Lines file.
// The data is used to match functions between binaries of different architectures
// (ELF x64 with symbols -> PE x86 without symbols).
//
// It also writes <output>.symbols.tsv with the writable data symbols
// (global variables), to map globals between binaries.
//
// Usage (headless): -postScript ExportFunctionFeatures.java <output.jsonl>
//@category SubVulpes

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.block.BasicBlockModel;
import ghidra.program.model.block.CodeBlockIterator;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.*;

import java.io.PrintWriter;
import java.util.*;

public class ExportFunctionFeatures extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String out = getScriptArgs().length > 0 ? getScriptArgs()[0] : askString("Output", ".jsonl file");
        Listing listing = currentProgram.getListing();
        long base = currentProgram.getImageBase().getOffset();
        BasicBlockModel bbm = new BasicBlockModel(currentProgram);

        try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
            for (Function f : listing.getFunctions(true)) {
                if (monitor.isCancelled()) break;
                if (f.isThunk() || f.isExternal()) continue;

                List<String> calls = new ArrayList<>();
                List<String> strings = new ArrayList<>();
                List<String> floats = new ArrayList<>();
                List<Long> ints = new ArrayList<>();
                List<String> globals = new ArrayList<>();
                int insns = 0;

                for (Instruction ins : listing.getInstructions(f.getBody(), true)) {
                    insns++;
                    if (ins.getFlowType().isCall()) {
                        for (Address to : ins.getFlows()) {
                            Function callee = getFunctionAt(to);
                            if (callee == null) continue;
                            if (callee.isThunk()) callee = callee.getThunkedFunction(true);
                            calls.add(callee.isExternal() ? "EXT:" + callee.getName()
                                    : String.format("0x%x", callee.getEntryPoint().getOffset() - base));
                        }
                    }
                    for (Reference ref : ins.getReferencesFrom()) {
                        if (!ref.getReferenceType().isData()) continue;
                        Data d = listing.getDataAt(ref.getToAddress());
                        if (d != null && StringDataInstance.isString(d)) {
                            Object v = d.getValue();
                            if (v != null) strings.add(v.toString());
                            continue;
                        }
                        String fv = readFloat(ref.getToAddress(), ins);
                        if (fv != null) { floats.add(fv); continue; }
                        MemoryBlock gb = currentProgram.getMemory().getBlock(ref.getToAddress());
                        if (gb != null && gb.isWrite() && !gb.isExecute())
                            globals.add(String.format("0x%x", ref.getToAddress().getOffset() - base));
                    }
                    for (int i = 0; i < ins.getNumOperands(); i++) {
                        for (Object o : ins.getOpObjects(i)) {
                            if (o instanceof Scalar) {
                                long v = ((Scalar) o).getSignedValue();
                                // Ignore small values (stack offsets, indices) and addresses.
                                if (Math.abs(v) >= 16 && Math.abs(v) < 0x400000) ints.add(v);
                            }
                        }
                    }
                }

                int blocks = 0;
                CodeBlockIterator it = bbm.getCodeBlocksContaining(f.getBody(), monitor);
                while (it.hasNext()) { it.next(); blocks++; }

                StringBuilder sb = new StringBuilder();
                sb.append("{\"rva\":\"").append(String.format("0x%x", f.getEntryPoint().getOffset() - base)).append('"');
                sb.append(",\"name\":").append(json(f.getName()));
                sb.append(",\"size\":").append(f.getBody().getNumAddresses());
                sb.append(",\"insns\":").append(insns);
                sb.append(",\"blocks\":").append(blocks);
                sb.append(",\"calls\":").append(jsonList(calls));
                sb.append(",\"strings\":").append(jsonList(strings));
                sb.append(",\"floats\":").append(jsonList(floats));
                sb.append(",\"ints\":").append(ints.toString());
                sb.append(",\"globals\":").append(jsonList(globals));
                sb.append('}');
                w.println(sb);
            }
        }
        // Named writable data symbols (they only exist in ELF/Mach-O).
        try (PrintWriter w = new PrintWriter(out + ".symbols.tsv", "UTF-8")) {
            SymbolIterator si = currentProgram.getSymbolTable().getAllSymbols(true);
            while (si.hasNext()) {
                Symbol sym = si.next();
                if (sym.getSource() == SourceType.DEFAULT || sym.isExternal()) continue;
                if (sym.getSymbolType() != SymbolType.LABEL) continue;
                MemoryBlock b = currentProgram.getMemory().getBlock(sym.getAddress());
                if (b == null || !b.isWrite() || b.isExecute()) continue;
                w.printf("0x%x	%s%n", sym.getAddress().getOffset() - base, sym.getName());
            }
        }
        println("Exported: " + out);
    }

    /** Read a float/double referenced by x87/SSE instructions in a read-only section. */
    private String readFloat(Address a, Instruction ins) {
        MemoryBlock b = currentProgram.getMemory().getBlock(a);
        if (b == null || !b.isInitialized() || b.isExecute()) return null;
        String m = ins.getMnemonicString().toUpperCase();
        try {
            // x87 in Ghidra: "FLD float ptr [..]" / "FMUL double ptr [..]". SSE: MOVSS, MULSD...
            String t = ins.toString();
            boolean fpu = m.startsWith("F");
            boolean dbl = m.endsWith("SD") || (fpu && (t.contains("double ptr") || t.contains("qword")));
            boolean flt = m.endsWith("SS") || (fpu && (t.contains("float ptr") || t.contains("dword")));
            if (dbl) return Double.toString(Double.longBitsToDouble(currentProgram.getMemory().getLong(a)));
            if (flt) return Double.toString(Float.intBitsToFloat(currentProgram.getMemory().getInt(a)));
        } catch (Exception e) {
            return null;
        }
        return null;
    }

    private static String json(String s) {
        StringBuilder sb = new StringBuilder("\"");
        for (char c : s.toCharArray()) {
            if (c == '"' || c == '\\') sb.append('\\').append(c);
            else if (c < 0x20 || c > 0x7e) sb.append(String.format("\\u%04x", (int) c));
            else sb.append(c);
        }
        return sb.append('"').toString();
    }

    private static String jsonList(List<String> l) {
        StringJoiner j = new StringJoiner(",", "[", "]");
        for (String s : l) j.add(json(s));
        return j.toString();
    }
}
