// Mostra as primeiras instruções de funções, com bytes e endereços absolutos.
// Serve para escolher o tamanho do prólogo de um hook (instruções inteiras, >= 5 bytes,
// sem salto relativo) e os bytes esperados.
//
// Uso (headless, -process <programa> -noanalysis):
//   -postScript PrintPrologue.java <saida.txt> <rva> [rva ...]
//@category SubVulpes

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;

import java.io.PrintWriter;

public class PrintPrologue extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        long base = currentProgram.getImageBase().getOffset();
        try (PrintWriter w = new PrintWriter(args[0], "UTF-8")) {
            for (int i = 1; i < args.length; i++) {
                long rva = Long.decode(args[i]);
                w.printf("== rva 0x%x%n", rva);
                Instruction ins = getInstructionAt(toAddr(base + rva));
                int total = 0;
                for (int n = 0; ins != null && n < 8; n++) {
                    StringBuilder hex = new StringBuilder();
                    for (byte b : ins.getBytes()) hex.append(String.format("%02x ", b));
                    boolean rel = ins.getFlowType().isJump() || ins.getFlowType().isCall();
                    StringBuilder refs = new StringBuilder();
                    for (Reference r : ins.getReferencesFrom()) {
                        if (r.getReferenceType().isData())
                            refs.append(String.format(" [abs rva 0x%x]", r.getToAddress().getOffset() - base));
                    }
                    total += ins.getLength();
                    w.printf("  +%2d  %-24s %-40s %s%s%n", total - ins.getLength(), hex, ins,
                            rel ? "RELATIVO" : "", refs);
                    ins = ins.getNext();
                }
            }
        }
    }
}
