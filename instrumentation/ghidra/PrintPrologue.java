// Show the first instructions of functions, with bytes and absolute addresses.
// Use it to choose the prologue size of a hook (whole instructions, >= 5 bytes,
// no relative jump) and the expected bytes.
//
// Usage (headless, -process <program> -noanalysis):
//   -postScript PrintPrologue.java <output.txt> <rva> [rva ...]
//   An argument "n40" sets how many instructions to print for the next addresses (default 8).
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
            int count = 8;
            for (int i = 1; i < args.length; i++) {
                if (args[i].startsWith("n")) {
                    count = Integer.parseInt(args[i].substring(1));
                    continue;
                }
                long rva = Long.decode(args[i]);
                w.printf("== rva 0x%x%n", rva);
                Instruction ins = getInstructionContaining(toAddr(base + rva));
                int total = 0;
                for (int n = 0; ins != null && n < count; n++) {
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
                            rel ? "RELATIVE" : "", refs);
                    ins = ins.getNext();
                }
            }
        }
    }
}
