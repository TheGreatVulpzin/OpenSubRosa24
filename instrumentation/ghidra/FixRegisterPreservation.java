// Tell the decompiler which registers each __cdecl function really keeps.
//
// The 024d was built with whole-program optimization. A caller keeps values in ECX, EDX
// and on the x87 stack across a call when the callee does not touch them. The normal
// __cdecl model says that a call destroys ECX, EDX, ST0 and ST1, so the decompiler shows
// these values as "extraout_EDX", "extraout_ST1"... and the pseudo-C cannot compile.
//
// This script:
//   1. Finds the registers that each function writes, with its callees (fixed point).
//      An import, an indirect call or an unknown target counts as "writes everything".
//   2. Adds variants of __cdecl to the program (compiler spec extensions) that keep
//      ECX, EDX and/or the x87 stack: __cdecl_keepC, __cdecl_keepD, __cdecl_keepF...
//   3. Gives each __cdecl function (or unknown convention with a plain RET) the variant that matches the registers it keeps.
//   4. Gives _ftol2 (float to 64-bit int, argument in ST0) its real signature and a call
//      fixup with its exact effect on the x87 stack.
//
// The lifted hooks keep ECX, EDX and the x87 state (see the thunks in hook/hooks_lifted.c),
// so a lifted function also respects the variant.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript FixRegisterPreservation.java
//@category SubVulpes

import ghidra.app.script.GhidraScript;
import ghidra.program.database.SpecExtension;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.Float10DataType;
import ghidra.program.model.data.UnsignedLongLongDataType;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.*;
import ghidra.program.model.pcode.PcodeOp;
import ghidra.program.model.pcode.Varnode;
import ghidra.program.model.symbol.FlowType;
import ghidra.program.model.symbol.SourceType;

import java.util.*;

public class FixRegisterPreservation extends GhidraScript {

    private static final int C = 1, D = 2, F = 4, ALL = 7;
    private static final long FTOL2_VA = 0x45ce90L;
    private static final String FTOL2_FIXUP = "<callfixup name=\"ftol2\"><target name=\"_ftol2\"/><pcode><body><![CDATA["
            // The CALL already pushed the return address: the RET of _ftol2 pops it (ESP + 4).
            + "ftol2tmp:8 = trunc(ST0); EAX = ftol2tmp:4; EDX = ftol2tmp(4); ESP = ESP + 4;"
            + "ST0 = ST1; ST1 = ST2; ST2 = ST3; ST3 = ST4; ST4 = ST5; ST5 = ST6; ST6 = ST7;"
            + "]]></body></pcode></callfixup>";

    @Override
    protected void run() throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        Map<Function, Integer> direct = new HashMap<>();
        Map<Function, Set<Function>> callees = new HashMap<>();

        for (Function f : fm.getFunctions(true)) {
            int w = 0;
            Set<Function> out = new HashSet<>();
            if (f.isThunk() || f.isExternal()) w = ALL;
            for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                if (w == ALL) break;
                for (PcodeOp op : ins.getPcode()) {
                    Varnode o = op.getOutput();
                    if (o != null && o.isRegister()) w |= regBits(o);
                }
                FlowType ft = ins.getFlowType();
                if (ft.isCall() || (ft.isJump() && !ft.isConditional())) {
                    Address[] flows = ins.getFlows();
                    if (ft.isComputed() && ft.isCall()) w = ALL;
                    if (ft.isCall() && flows.length == 0) w = ALL;
                    for (Address a : flows) {
                        if (f.getBody().contains(a) && !ft.isCall()) continue;
                        Function g = fm.getFunctionAt(a);
                        if (g == null) g = fm.getFunctionContaining(a);
                        if (g == null || g.isExternal() || g.isThunk()) w = ALL;
                        else if (g != f) out.add(g);
                    }
                    if (ft.isJump() && ft.isComputed() && flows.length == 0) w = ALL;
                }
            }
            direct.put(f, w);
            callees.put(f, out);
        }

        Map<Function, Integer> touched = new HashMap<>(direct);
        boolean changed = true;
        while (changed) {
            changed = false;
            for (Function f : touched.keySet()) {
                int w = touched.get(f);
                for (Function g : callees.get(f)) w |= touched.getOrDefault(g, ALL);
                if (w != touched.get(f)) {
                    touched.put(f, w);
                    changed = true;
                }
            }
        }

        SpecExtension ext = new SpecExtension(currentProgram);
        for (int keep = 1; keep <= ALL; keep++) {
            ext.addReplaceCompilerSpecExtension(prototypeXml(keep), monitor);
        }

        Function ftol2 = fm.getFunctionAt(toAddr(FTOL2_VA));
        int[] count = new int[ALL + 1];
        for (Function f : fm.getFunctions(true)) {
            if (f == ftol2 || f.hasCustomVariableStorage()) continue;
            String cc = f.getCallingConventionName();
            boolean unknown = cc.equals("unknown") || cc.equals("default");
            if (!cc.equals("__cdecl") && !cc.startsWith("__cdecl_keep") && !unknown) continue;
            // An unknown convention is __cdecl only when every RET leaves the arguments on the stack.
            if (unknown && !callerCleans(f)) continue;
            int keep = ALL & ~touched.get(f);
            String want = keep == 0 ? "__cdecl" : protoName(keep);
            if (!want.equals(cc)) f.setCallingConvention(want);
            count[keep]++;
        }
        for (int keep = 0; keep <= ALL; keep++) {
            println((keep == 0 ? "__cdecl" : protoName(keep)) + ": " + count[keep]);
        }

        if (ftol2 != null) {
            Register st0 = currentProgram.getRegister("ST0");
            Register eax = currentProgram.getRegister("EAX");
            Register edx = currentProgram.getRegister("EDX");
            ReturnParameterImpl ret = new ReturnParameterImpl(UnsignedLongLongDataType.dataType,
                    new VariableStorage(currentProgram, edx, eax), currentProgram);
            List<Variable> params = new ArrayList<>();
            params.add(new ParameterImpl("x", Float10DataType.dataType,
                    new VariableStorage(currentProgram, st0), currentProgram));
            ftol2.setName("_ftol2", SourceType.USER_DEFINED);
            ftol2.updateFunction(null, ret, params, Function.FunctionUpdateType.CUSTOM_STORAGE, true,
                    SourceType.USER_DEFINED);
            // The call pops ST0 and returns its truncated value in EDX:EAX. A call fixup shows
            // this effect inline, so the callers see the rest of the x87 stack move up.
            ext.addReplaceCompilerSpecExtension(FTOL2_FIXUP, monitor);
            ftol2.setCallFixup("ftol2");
            println("fixed _ftol2");
        }
    }

    private boolean callerCleans(Function f) {
        boolean anyRet = false;
        for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
            if (!ins.getMnemonicString().startsWith("RET")) continue;
            anyRet = true;
            if (ins.getNumOperands() > 0) return false;
        }
        return anyRet;
    }

    private int regBits(Varnode v) {
        Register r = currentProgram.getRegister(v);
        if (r == null) return 0;
        String n = r.getBaseRegister().getName();
        if (n.equals("ECX")) return C;
        if (n.equals("EDX")) return D;
        if (n.matches("ST[0-7]")) return F;
        return 0;
    }

    private static String protoName(int keep) {
        return "__cdecl_keep" + ((keep & C) != 0 ? "C" : "") + ((keep & D) != 0 ? "D" : "")
                + ((keep & F) != 0 ? "F" : "");
    }

    /** __cdecl from x86win.cspec, with the kept registers moved from killedbycall to unaffected. */
    private static String prototypeXml(int keep) {
        StringBuilder unaff = new StringBuilder();
        StringBuilder killed = new StringBuilder("<register name=\"EAX\"/>");
        if ((keep & C) != 0) unaff.append("<register name=\"ECX\"/>");
        else killed.append("<register name=\"ECX\"/>");
        if ((keep & D) != 0) unaff.append("<register name=\"EDX\"/>");
        else killed.append("<register name=\"EDX\"/>");
        if ((keep & F) != 0) {
            for (int i = 0; i < 8; i++) unaff.append("<register name=\"ST").append(i).append("\"/>");
        } else {
            killed.append("<register name=\"ST0\"/><register name=\"ST1\"/>");
        }
        return "<prototype name=\"" + protoName(keep) + "\" extrapop=\"4\" stackshift=\"4\">"
                + "<input><pentry minsize=\"1\" maxsize=\"500\" align=\"4\"><addr offset=\"4\" space=\"stack\"/></pentry></input>"
                + "<output>"
                + "<pentry minsize=\"4\" maxsize=\"10\" metatype=\"float\" extension=\"float\"><register name=\"ST0\"/></pentry>"
                + "<pentry minsize=\"1\" maxsize=\"4\"><register name=\"EAX\"/></pentry>"
                // A function that keeps EDX cannot return 64 bits in EDX:EAX. Without this rule
                // the decompiler still shows the kept EDX as "extraout_EDX".
                + ((keep & D) != 0 ? ""
                        : "<pentry minsize=\"5\" maxsize=\"8\"><addr space=\"join\" piece1=\"EDX\" piece2=\"EAX\"/></pentry>")
                + "</output>"
                + "<unaffected><varnode space=\"ram\" offset=\"0\" size=\"4\"/>"
                + "<register name=\"ESP\"/><register name=\"EBP\"/><register name=\"ESI\"/><register name=\"EDI\"/>"
                + "<register name=\"EBX\"/><register name=\"DF\"/><register name=\"FS_OFFSET\"/>"
                + unaff + "</unaffected>"
                + "<killedbycall>" + killed + "</killedbycall>"
                + "</prototype>";
    }
}
