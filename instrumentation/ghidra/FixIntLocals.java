// Give the type int to float locals that are integers.
//
// The decompiler can type an integer local as float: an index that shares its stack slot
// with a float, for example. The lifted C is still correct, because each use goes through
// LF_BITS_U / LF_BITS_F, but the value stays on the x87. A small integer read as a float is
// a denormal, and an x87 load of a denormal takes about 100 cycles. In the object pair loop
// of objectsimulation (1024 x 1024 passes each tick) the standalone server ran at 9 ticks/s.
//
// Two ways a local changes:
// 1. Automatic: no p-code op of any of its varnodes is a float op (FLOAT_*, INT2FLOAT,
//    FLOAT2FLOAT, TRUNC...). The value is only copied, compared for equality or used in
//    integer arithmetic, and an int of the same size has the same bits.
// 2. Listed in analysis/024d/locals.csv (rva, local, type, evidence): a local that also
//    holds a float in another part of the function, or a pointer with the wrong type (a
//    float * that walks an array of structs). An automatic rule for these broke
//    humansimulation and setupobjectypes_train, so each one is checked by hand. The local
//    names are the ones of the first decompile, before this script changes anything.
//
// Run it once after ApplyTypes.java: each change can change how the decompiler merges the
// other locals, so a second run is not the same as the first.
//
// Headless (writes the project): -postScript FixIntLocals.java [explain] [rva ...]
// "explain" prints why each float local stays a float. No rva: all functions.
// @category SubVulpes
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.IntegerDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunction;
import ghidra.program.model.pcode.HighFunctionDBUtil;
import ghidra.program.model.pcode.HighSymbol;
import ghidra.program.model.pcode.HighVariable;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.symbol.SourceType;

public class FixIntLocals extends GhidraScript {

    /* "rva:local" -> type, from analysis/024d/locals.csv. */
    private final Map<String, String> listed = new HashMap<>();

    @Override
    protected void run() throws Exception {
        long base = currentProgram.getImageBase().getOffset();
        Path csv = Paths.get("analysis/024d/locals.csv");
        if (Files.exists(csv))
            for (String line : Files.readAllLines(csv)) {
                String[] c = line.split(",", 4);
                if (c.length < 3 || c[0].equals("rva")) continue;
                listed.put(Long.decode(c[0]) + ":" + c[1], c[2].trim());
            }
        String[] args = getScriptArgs();
        boolean explain = args.length > 0 && args[0].equals("explain");
        if (explain) args = Arrays.copyOfRange(args, 1, args.length);
        List<Function> funcs = new ArrayList<>();
        if (args.length == 0) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) funcs.add(f);
        } else {
            for (String a : args) {
                Function f = getFunctionAt(toAddr(base + Long.decode(a)));
                if (f != null) funcs.add(f);
            }
        }
        ghidra.util.data.DataTypeParser parser = new ghidra.util.data.DataTypeParser(
                currentProgram.getDataTypeManager(), null, null,
                ghidra.util.data.DataTypeParser.AllowedDataTypes.ALL);
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        int changed = 0, fromList = 0;
        for (Function f : funcs) {
            if (monitor.isCancelled()) break;
            DecompileResults r = di.decompileFunction(f, 60, monitor);
            if (!r.decompileCompleted()) continue;
            HighFunction hf = r.getHighFunction();
            long rva = f.getEntryPoint().getOffset() - base;
            List<HighSymbol> fix = new ArrayList<>();
            Map<HighSymbol, DataType> fixType = new HashMap<>();
            Iterator<HighSymbol> it = hf.getLocalSymbolMap().getSymbols();
            while (it.hasNext()) {
                HighSymbol s = it.next();
                if (s.isParameter()) continue;
                DataType dt = s.getDataType();
                String want = listed.get(rva + ":" + s.getName());
                // A listed local that is not a float (the float * that walks the bonds in
                // bondsimulation2): the type of the list, as written there.
                if (want != null && !want.equals("int") && dt != null) {
                    fixType.put(s, parser.parse(want));
                    fromList++;
                    continue;
                }
                if (dt == null || dt.getLength() != 4 || !dt.getName().equals("float")) continue;
                HighVariable hv = s.getHighVariable();
                if (hv == null) continue;
                String why = floatUse(hv);
                boolean inList = listed.containsKey(rva + ":" + s.getName());
                if (why == null || inList) {
                    fix.add(s);
                    if (why != null) fromList++;
                } else if (explain) {
                    println(String.format("  %s: %s stays float%s: %s", f.getName(), s.getName(),
                            intArithmetic(hv) ? " (CANDIDATE: also integer arithmetic)" : "", why));
                }
            }
            for (HighSymbol s : fix) {
                HighFunctionDBUtil.updateDBVariable(s, null, IntegerDataType.dataType, SourceType.USER_DEFINED);
                println(String.format("%s: %s float -> int", f.getName(), s.getName()));
                changed++;
            }
            for (Map.Entry<HighSymbol, DataType> e : fixType.entrySet()) {
                println(String.format("%s: %s %s -> %s", f.getName(), e.getKey().getName(),
                        e.getKey().getDataType().getDisplayName(), e.getValue().getDisplayName()));
                HighFunctionDBUtil.updateDBVariable(e.getKey(), null, e.getValue(), SourceType.USER_DEFINED);
                changed++;
            }
        }
        di.dispose();
        println(String.format("FixIntLocals: %d locals changed (%d from locals.csv) in %d functions", changed,
                fromList, funcs.size()));
    }

    private static boolean isFloatOp(int code) {
        switch (code) {
            case PcodeOp.FLOAT_ADD: case PcodeOp.FLOAT_SUB: case PcodeOp.FLOAT_MULT: case PcodeOp.FLOAT_DIV:
            case PcodeOp.FLOAT_NEG: case PcodeOp.FLOAT_ABS: case PcodeOp.FLOAT_SQRT: case PcodeOp.FLOAT_CEIL:
            case PcodeOp.FLOAT_FLOOR: case PcodeOp.FLOAT_ROUND: case PcodeOp.FLOAT_NAN: case PcodeOp.FLOAT_EQUAL:
            case PcodeOp.FLOAT_NOTEQUAL: case PcodeOp.FLOAT_LESS: case PcodeOp.FLOAT_LESSEQUAL:
            case PcodeOp.FLOAT_INT2FLOAT: case PcodeOp.FLOAT_FLOAT2FLOAT: case PcodeOp.FLOAT_TRUNC:
                return true;
            default:
                return false;
        }
    }

    private static boolean isIntArith(int code) {
        switch (code) {
            case PcodeOp.INT_ADD: case PcodeOp.INT_SUB: case PcodeOp.INT_MULT: case PcodeOp.INT_SDIV:
            case PcodeOp.INT_LESS: case PcodeOp.INT_SLESS: case PcodeOp.INT_LESSEQUAL: case PcodeOp.INT_SLESSEQUAL:
            case PcodeOp.PTRADD:
                return true;
            default:
                return false;
        }
    }

    /* For "explain": an op of integer arithmetic reads the variable, directly or through a
     * CAST. Such a local is a candidate for analysis/024d/locals.csv. */
    private static boolean intArithmetic(HighVariable hv) {
        for (Varnode v : hv.getInstances()) {
            Iterator<PcodeOp> d = v.getDescendants();
            while (d.hasNext()) {
                PcodeOp op = d.next();
                if (isIntArith(op.getOpcode())) return true;
                if (op.getOpcode() == PcodeOp.CAST && op.getOutput() != null) {
                    Iterator<PcodeOp> d2 = op.getOutput().getDescendants();
                    while (d2.hasNext())
                        if (isIntArith(d2.next().getOpcode())) return true;
                }
            }
        }
        return false;
    }

    /* The first float op that reads or writes one of the varnodes of the variable, or null.
     * A CALL argument also counts: the callee can read it as a float. */
    private static String floatUse(HighVariable hv) {
        for (Varnode v : hv.getInstances()) {
            PcodeOp def = v.getDef();
            if (def != null && isFloatOp(def.getOpcode())) return "written by " + def;
            Iterator<PcodeOp> d = v.getDescendants();
            while (d.hasNext()) {
                PcodeOp op = d.next();
                if (isFloatOp(op.getOpcode()) || op.getOpcode() == PcodeOp.CALL) return "read by " + op;
            }
        }
        return null;
    }
}
