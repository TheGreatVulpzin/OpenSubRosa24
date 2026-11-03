// Tell the decompiler that __security_check_cookie keeps EAX and the x87 stack.
//
// A function that ends with a call to __security_check_cookie looks like a void function:
// the decompiler thinks the call destroys EAX and ST0. Its callers then read a value from a
// "void" function, and the lifted C does not compile (C2069, C2186). The call fixup
// "security_check_cookie" of x86win.cspec makes the decompiler ignore the call. The return
// type then comes from the code before it.
//
// The function is found by name, so the script works for any program.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript FixCookieCheck.java
//@category SubVulpes

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class FixCookieCheck extends GhidraScript {
    @Override
    protected void run() throws Exception {
        int n = 0;
        FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
        while (it.hasNext()) {
            Function f = it.next();
            String name = f.getName();
            if (name.equals("__security_check_cookie") || name.equals("@__security_check_cookie@4")) {
                f.setCallFixup("security_check_cookie");
                println("call fixup on " + name + " at " + f.getEntryPoint());
                n++;
            }
        }
        println("cookie fixups: " + n);
    }
}
