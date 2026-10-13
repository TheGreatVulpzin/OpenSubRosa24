// Print the decompiler tokens of some functions, with the p-code op of each token and the
// type of its variable. A debug aid for the rules in LiftExport.java.
//
// Usage (headless, -process <program> -noanalysis -readOnly):
//   -postScript DumpTokens.java <output.txt> <rva> [<rva> ...]
//@category SubVulpes

import ghidra.app.decompiler.ClangNode;
import ghidra.app.decompiler.ClangToken;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.Varnode;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

public class DumpTokens extends GhidraScript {

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        DecompInterface di = new DecompInterface();
        di.setOptions(new DecompileOptions());
        di.openProgram(currentProgram);
        try (PrintWriter w = new PrintWriter(args[0], "UTF-8")) {
            for (int a = 1; a < args.length; a++) {
                long rva = Long.decode(args[a]);
                Function f = getFunctionAt(currentProgram.getImageBase().add(rva));
                if (f == null) {
                    w.println("no function at " + args[a]);
                    continue;
                }
                DecompileResults r = di.decompileFunction(f, 120, monitor);
                List<ClangNode> leaves = new ArrayList<>();
                r.getCCodeMarkup().flatten(leaves);
                w.println("== " + f.getName() + " " + args[a]);
                for (ClangNode n : leaves) {
                    ClangToken t = (ClangToken) n;
                    if (t.getText().isBlank()) continue;
                    StringBuilder s = new StringBuilder();
                    s.append(String.format("%-24s %-20s", t.getText(), t.getClass().getSimpleName()));
                    PcodeOp op = t.getPcodeOp();
                    if (op != null) {
                        s.append(" op=").append(op.getMnemonic());
                        Varnode o = op.getOutput();
                        if (o != null && o.getHigh() != null)
                            s.append(" out=").append(o.getHigh().getDataType().getName()).append('/').append(o.getSize());
                        for (int i = 0; i < op.getNumInputs(); i++) {
                            Varnode v = op.getInput(i);
                            s.append(" in").append(i).append('=')
                                    .append(v.getHigh() == null ? "?" : v.getHigh().getDataType().getName())
                                    .append('/').append(v.getSize());
                        }
                    }
                    if (t.getHighVariable() != null)
                        s.append(" var=").append(t.getHighVariable().getDataType().getName());
                    w.println(s);
                }
            }
        }
        di.dispose();
    }
}
