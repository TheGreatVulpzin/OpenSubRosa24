// Give an int return type to functions that Ghidra typed as void, but whose callers read EAX.
//
// Most of these functions end with a call to __security_check_cookie. The decompiler
// thinks that the call destroys EAX, so it decides that the function returns nothing.
// The original callers read EAX after the call, so the function returns a value there.
// (The callers that write "(float10)FUN_x(...)" convert this int to float: the functions
// with a __cdecl_keepF convention do not touch the x87 stack.)
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

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.Undefined4DataType;
import ghidra.program.model.listing.Function;
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
            println(String.format("0x%x %s: return %s -> undefined4", rva, f.getName(), f.getReturnType().getName()));
            f.setReturnType(Undefined4DataType.dataType, SourceType.USER_DEFINED);
            n++;
        }
        println("int returns fixed: " + n);
    }
}
