// Give each call of a variable-argument function the exact number of arguments.
//
// A function such as createmenubutton(int type, ...) or sprintf(buffer, format, ...) reads its extra
// arguments by type or by format. The decompiler guesses the extra arguments of each call and
// sometimes drops all of them (a pointer to a local, a format held in a global, or a constant when
// the compiler left the stack cleanup to the next call). The lifted C then calls the function with
// too few arguments and the function reads stack garbage: a wrong amount of money on the screen.
// The machine code is exact: the caller pushes the arguments right before the call and removes them
// with "add esp, N" or "pop". This script counts the pushed words and writes a signature override
// on each call. The fixed parameters keep their types. Each extra word is an int.
//
// The CSV has one row per function: rva, name. An empty rva means an import: the script finds each
// call of the import by its name. Only cdecl functions whose callers clean the stack.
//
// Usage (headless, -process <program> -noanalysis, WITHOUT -readOnly so the change is saved):
//   -postScript ApplyCallOverrides.java <path of call_overrides.csv>
//@category SubVulpes

import java.io.BufferedReader;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.data.IntegerDataType;
import ghidra.program.model.data.ParameterDefinition;
import ghidra.program.model.data.ParameterDefinitionImpl;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.pcode.HighFunctionDBUtil;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;

public class ApplyCallOverrides extends GhidraScript {
    /**
     * The words pushed in the straight code right before the call. The compiler puts other
     * instructions between the pushes and may clean only part of them after the call, so the
     * scan walks back over the whole block. It stops at a jump, a call, a return or a stack cleanup.
     * A "sub esp, N" of 16 bytes or less is an argument slot (a double is stored with "fstp [esp]").
     */
    private int pushesBefore(Instruction call) {
        int n = 0;
        Instruction prev = call.getPrevious();
        for (int guard = 0; prev != null && guard < 80; guard++, prev = prev.getPrevious()) {
            String m = prev.getMnemonicString();
            if (prev.getFlowType().isJump() || prev.getFlowType().isCall() || prev.getFlowType().isTerminal())
                break;
            if (m.equals("POP") || m.equals("LEAVE"))
                break;
            if ((m.equals("ADD") || m.equals("SUB")) && prev.getNumOperands() == 2 && prev.getRegister(0) != null
                    && prev.getRegister(0).getName().equals("ESP")) {
                Object[] op = prev.getOpObjects(1);
                if (m.equals("SUB") && op.length == 1 && op[0] instanceof Scalar
                        && ((Scalar) op[0]).getUnsignedValue() <= 16)
                    n += (int) (((Scalar) op[0]).getUnsignedValue() / 4);
                else
                    break;
                continue;
            }
            if (m.equals("PUSH"))
                n++;
        }
        return n;
    }

    /** The words that the code after the call removes, or -1 when it is not a plain cleanup. */
    private int cleanupWords(Instruction call) {
        Instruction next = call.getNext();
        if (next == null)
            return -1;
        String m = next.getMnemonicString();
        if (m.equals("ADD") && next.getNumOperands() == 2 && next.getRegister(0) != null
                && next.getRegister(0).getName().equals("ESP")) {
            Object[] op = next.getOpObjects(1);
            if (op.length == 1 && op[0] instanceof Scalar)
                return (int) (((Scalar) op[0]).getUnsignedValue() / 4);
        }
        if (m.equals("POP") && next.getRegister(0) != null) {
            int n = 0;
            while (next != null && next.getMnemonicString().equals("POP")) {
                n++;
                next = next.getNext();
            }
            return n;
        }
        return -1;
    }

    /** The words a caller pushed for one call: the larger of the cleanup after the call and the pushes before it. */
    private int pushedWords(Instruction call) {
        return Math.max(cleanupWords(call), pushesBefore(call));
    }

    /** The function that a call reaches. An import is reached through its thunk or its pointer slot. */
    private Function target(Address to) {
        Function f = getFunctionAt(to);
        if (f == null) {
            for (Reference r : currentProgram.getReferenceManager().getReferencesFrom(to)) {
                f = getFunctionAt(r.getToAddress());
                if (f != null)
                    break;
            }
        }
        if (f != null && f.isThunk())
            f = f.getThunkedFunction(true);
        return f;
    }

    private int fixedWords(Function f) {
        int n = 0;
        for (Parameter p : f.getParameters())
            n += Math.max(4, (p.getLength() + 3) / 4 * 4) / 4;
        return n;
    }

    private boolean override(Instruction call, Function callee, int words) throws Exception {
        int fixed = fixedWords(callee);
        if (words <= fixed)
            return false;
        List<ParameterDefinition> params = new ArrayList<>();
        int used = 0;
        for (Parameter p : callee.getParameters()) {
            params.add(new ParameterDefinitionImpl(p.getName(), p.getDataType(), null));
            used += Math.max(4, (p.getLength() + 3) / 4 * 4) / 4;
        }
        for (int i = used; i < words; i++)
            params.add(new ParameterDefinitionImpl("a" + i, IntegerDataType.dataType, null));
        FunctionDefinitionDataType sig = new FunctionDefinitionDataType("call_override");
        sig.setReturnType(callee.getReturnType());
        sig.setArguments(params.toArray(new ParameterDefinition[0]));
        sig.setCallingConvention("__cdecl");
        HighFunctionDBUtil.writeOverride(getFunctionContaining(call.getAddress()), call.getAddress(), sig);
        return true;
    }

    @Override
    protected void run() throws Exception {
        String csv = getScriptArgs()[0];
        Map<String, Integer> done = new HashMap<>();
        Map<Address, String> wanted = new HashMap<>();
        List<String> importNames = new ArrayList<>();
        try (BufferedReader in = new BufferedReader(new FileReader(csv))) {
            in.readLine();  // header
            String line;
            while ((line = in.readLine()) != null) {
                String[] f = line.split(",");
                if (f.length < 2)
                    continue;
                if (f[0].trim().isEmpty())
                    importNames.add(f[1].trim());
                else
                    wanted.put(currentProgram.getImageBase().add(Long.decode(f[0].trim())), f[1].trim());
            }
        }
        int total = 0, skipped = 0;
        InstructionIterator it = currentProgram.getListing().getInstructions(true);
        while (it.hasNext()) {
            Instruction call = it.next();
            if (!call.getFlowType().isCall())
                continue;
            for (Reference ref : call.getReferencesFrom()) {
                if (!ref.getReferenceType().isCall())
                    continue;
                Function callee = target(ref.getToAddress());
                if (callee == null)
                    continue;
                String name = callee.getName();
                boolean byAddress = wanted.containsKey(callee.getEntryPoint());
                if (!byAddress && !importNames.contains(name))
                    continue;
                int words = pushedWords(call);
                if (words < 1) {
                    println("skip call at " + call.getAddress() + " to " + name + " (no pushes)");
                    skipped++;
                    continue;
                }
                if (override(call, callee, words)) {
                    done.merge(name, 1, Integer::sum);
                    total++;
                }
                break;
            }
        }
        for (Map.Entry<String, Integer> e : done.entrySet())
            println("overrides for " + e.getKey() + ": " + e.getValue());
        println("call overrides written: " + total + ", skipped " + skipped);
    }
}
