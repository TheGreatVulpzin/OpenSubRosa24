// Exporta tudo que o conversor automático (tools/lift/lift.py) precisa, em JSON Lines.
//
// Por função: RVA, nome, convenção de chamada, retorno, parâmetros (com o lugar de cada um),
// pseudo-C, globais com tipo, funções chamadas com assinatura, e o prólogo para o hook
// (bytes, instruções inteiras >= 5 bytes, posições relocadas pelo ASLR).
//
// Uso (headless, -process <programa> -noanalysis): -postScript LiftExport.java <saida.jsonl>
//@category SubVulpes

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighSymbol;
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
                if (r.decompileCompleted()) globals(sb, r.getHighFunction());
                sb.append("],\"calls\":[");
                calls(sb, f);
                sb.append("],");
                prologue(sb, f);
                sb.append('}');
                w.println(sb);
            }
        }
        di.dispose();
        println("Exportado: " + out);
    }

    private void globals(StringBuilder sb, HighFunction hf) {
        boolean first = true;
        Iterator<HighSymbol> it = hf.getGlobalSymbolMap().getSymbols();
        while (it.hasNext()) {
            HighSymbol s = it.next();
            Address a = s.getStorage().isMemoryStorage() ? s.getStorage().getMinAddress() : null;
            if (a == null) continue;
            DataType dt = s.getDataType();
            if (!first) sb.append(',');
            first = false;
            sb.append('{');
            field(sb, "name", s.getName()).append(',');
            field(sb, "va", String.format("0x%x", a.getOffset())).append(',');
            field(sb, "type", dt.getDisplayName()).append(',');
            sb.append("\"size\":").append(dt.getLength());
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

    /* Prólogo: instruções inteiras até >= 5 bytes. Marca salto relativo e posições relocadas. */
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
