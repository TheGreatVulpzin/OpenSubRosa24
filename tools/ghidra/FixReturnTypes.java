// Give an int return type to functions that Ghidra typed as void, but whose callers read EAX.
//
// Most of these functions end with a call to __security_check_cookie. The decompiler
// thinks that the call destroys EAX, so it decides that the function returns nothing.
// The original callers read EAX after the call, so the function returns a value there.
//
// The script stores the parameters that the decompiler finds, then locks an int return.
// Two mistakes to avoid:
// - An undefined4 return can match ST0: the __cdecl_keepF prototype lists ST0 before EAX
//   as an output. setupobjecttypes_helicopters loads 1.0 (fld1) before calls to
//   objecttype_addbond, and the pseudo-C showed "fVar8 = (float10)FUN_0040b0b0(...)".
// - A locked signature with no stored parameters makes every caller drop its arguments.
//
// The script also applies the "security_check_cookie" call fixup of x86win.cspec to
// __security_check_cookie (0x45ce18): the decompiler then ignores the call.
//
// The list comes from tools/lift (callers with "= FUN_x(" for a callee typed void, checked
// in the disassembly: every return path sets EAX).
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript FixReturnTypes.java
//@category SubVulpes

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.IntegerDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.pcode.HighFunctionDBUtil;
import ghidra.program.model.symbol.SourceType;

public class FixReturnTypes extends GhidraScript {

    private static final long[] INT_RETURNS = {
            0x65f0, 0x6760, 0xb0b0, 0xded0, 0x1ac40, 0x29bf0, 0x2c0c0, 0x33350, 0x40aa0, 0x41550, 0x58320,
            0x587f0, 0x5cd10, 0x6380 };
    private static final long COOKIE_RVA = 0x5ce18;

    @Override
    protected void run() throws Exception {
        long base = currentProgram.getImageBase().getOffset();
        Function cookie = getFunctionAt(toAddr(base + COOKIE_RVA));
        if (cookie != null) {
            cookie.setCallFixup("security_check_cookie");
            println("call fixup on " + cookie.getName());
        }
        int n = 0;
        for (long rva : INT_RETURNS) {
            Function f = getFunctionAt(toAddr(base + rva));
            if (f == null) {
                println(String.format("no function at 0x%x", rva));
                continue;
            }
            println(String.format("0x%x %s: return %s -> int", rva, f.getName(), f.getReturnType().getName()));
            f.setSignatureSource(SourceType.DEFAULT);
            DecompInterface di = new DecompInterface();
            di.setOptions(new DecompileOptions());
            di.openProgram(currentProgram);
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            if (r.decompileCompleted()) {
                HighFunctionDBUtil.commitParamsToDatabase(r.getHighFunction(), true,
                        HighFunctionDBUtil.ReturnCommitOption.NO_COMMIT, SourceType.USER_DEFINED);
                f.setReturnType(IntegerDataType.dataType, SourceType.USER_DEFINED);
                println("  parameters stored: " + f.getParameterCount());
            }
            di.dispose();
            n++;
        }
        println("int returns fixed: " + n);
    }
}
