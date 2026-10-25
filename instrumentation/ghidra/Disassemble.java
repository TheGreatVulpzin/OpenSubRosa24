// Print the disassembly of functions, by RVA.
//
// Usage (headless, -readOnly): -postScript Disassemble.java 0x10da0 [0xf6f0 ...]
//@category SubVulpes

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;

public class Disassemble extends GhidraScript {
    @Override
    protected void run() throws Exception {
        Address base = currentProgram.getImageBase();
        for (String arg : getScriptArgs()) {
            Address a = base.add(Long.decode(arg));
            Function f = getFunctionAt(a);
            if (f == null) {
                println("no function at " + arg);
                continue;
            }
            StringBuilder sb = new StringBuilder("\n== " + f.getName() + " (" + f.getCallingConventionName() + ")\n");
            for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                sb.append(String.format("  +0x%05x  %s%n", ins.getAddress().subtract(base), ins));
            }
            println(sb.toString());
        }
    }
}
