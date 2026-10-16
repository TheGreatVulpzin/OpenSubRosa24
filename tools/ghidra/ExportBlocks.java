// Export the basic blocks of each function: one line per function,
// "<entry rva> <block rva> <block rva> ..." (hex, relative to the image base).
// The harness coverage mode (HARNESS_COVER) puts an INT3 on each block start.
//
// Headless: -postScript ExportBlocks.java <output file>
// @category SubVulpes
import java.io.PrintWriter;
import java.util.TreeSet;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.block.BasicBlockModel;
import ghidra.program.model.block.CodeBlock;
import ghidra.program.model.block.CodeBlockIterator;
import ghidra.program.model.listing.Function;

public class ExportBlocks extends GhidraScript {
    @Override
    protected void run() throws Exception {
        String path = getScriptArgs().length > 0 ? getScriptArgs()[0] : "build/lift/blocks.txt";
        long base = currentProgram.getImageBase().getOffset();
        BasicBlockModel model = new BasicBlockModel(currentProgram);
        int funcs = 0, blocks = 0;
        try (PrintWriter w = new PrintWriter(path)) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                TreeSet<Long> starts = new TreeSet<>();
                CodeBlockIterator it = model.getCodeBlocksContaining(f.getBody(), monitor);
                while (it.hasNext()) {
                    CodeBlock b = it.next();
                    starts.add(b.getFirstStartAddress().getOffset() - base);
                }
                StringBuilder sb = new StringBuilder(Long.toHexString(f.getEntryPoint().getOffset() - base));
                for (long s : starts)
                    sb.append(' ').append(Long.toHexString(s));
                w.println(sb);
                funcs++;
                blocks += starts.size();
            }
        }
        println(String.format("Exported %d functions, %d blocks to %s", funcs, blocks, path));
    }
}
