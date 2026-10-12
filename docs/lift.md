# Automatic lifting (Phase 3)

`tools/lift/lift.py` turns the Ghidra pseudo-C of the 024d into C that compiles with MSVC, and generates a hook with verification for each function.

## Pipeline

0. Once per Ghidra project, fix the signatures (run without `-readOnly`, so the changes are saved):

   ```bash
   "$HEADLESS" "$PROJECT" subvulpes -process subrosadedicated024d.exe -noanalysis \
       -scriptPath "$ROOT/tools/ghidra" -postScript FixX87Intrinsics.java -postScript FixRegisterPreservation.java
   ```

1. Export from Ghidra (5 minutes):

   ```bash
   source tools/ghidra/env.sh
   "$HEADLESS" "$PROJECT" subvulpes -process subrosadedicated024d.exe -noanalysis -readOnly \
       -scriptPath "$ROOT/tools/ghidra" -postScript LiftExport.java "$ROOT/build/lift/ded024d.jsonl"
   ```

   Per function: pseudo-C, globals with the type the decompiler used, callees with prototypes, the prologue for the hook, and the values at relocation sites.

2. Lift and compile (2 to 5 minutes):

   ```bash
   python -I tools/lift/lift.py --compile
   ```

   Output: `src/lifted/lifted_024d.c`, `hook/hooks_lifted.c`, `build/lift/report.csv`. The compile loop removes functions that do not compile and tries again.

3. Build the DLL (`tools\build_hook.bat`) and run the automatic test:

   ```bat
   powershell -File tools\autotest.ps1 -Seconds 180
   ```

## What the lifter does

| Problem | Solution |
| --- | --- |
| Ghidra types (`undefined4`, `float10`...) and helper ops (`CONCAT31`, `SUB41`, `ROUND`...) | `src/lifted/lift_support.h` |
| Globals (`DAT_0f837318`) | One `#define` per global with the decompiler type, relocated with `SR_VA()` |
| Partial access (`x._4_2_`) | Pointer access with the right size |
| Calls to other game functions | Through the game address (so hooks and originals both work) |
| CRT and Windows imports | Through the game IAT (`sr_import`), so `rand()`, `FILE*` and the heap are the game ones |
| Absolute addresses shown as numbers (`ptr < 0x46177c`) | The values at relocation sites become `SR_VA(...)` |

## What the 024d does that breaks the normal ABI

The 024d was built with whole-program optimization. Callers trust details of each callee:

- **ECX and EDX survive calls** when the original callee does not touch them. A caller in `addbeziersurface` keeps a loop pointer in EDX across a call. Our C code changed EDX, and the server crashed later in original code.
- **Live x87 values survive calls.** Callers keep values on the x87 stack across calls.

Each lifted hook therefore goes through a naked thunk (generated in `hook/hooks_lifted.c`):

1. `FXSAVE` the caller x87 state and save EAX, ECX and EDX.
2. `FNINIT` and the same control word: the C code gets an empty x87 stack.
3. Call the verification wrapper.
4. `FXRSTOR`, push a float result back on top, and restore the registers that do not carry the return value.

## Why a function is not lifted

`build/lift/report.csv` lists the status and the reason for each function. The main reasons:

| Reason | Meaning |
| --- | --- |
| `side_effect` | The function reaches the network, files, time, the heap or another impure import (directly or through callees). Verification runs a function twice, so these cannot be verified this way. |
| `register input/output` | The decompiler shows values passed in registers (non-standard convention). Most cases are gone after `FixRegisterPreservation.java`. The rest are CRT helpers and callees that return a second value in EDX. |
| `int typed as float (denormal literal)` | Ghidra typed an int parameter as float (`param_1 == 1.4013e-45`). |
| `compile_error` | The pseudo-C does not compile. |

## Results

First complete run (2026-10-12): 141 of 340 functions lifted and compiled. Automatic test on an idle server (180 s): no crash, 10 lifted functions called and verified, 0 differences.

With the x87 intrinsics (2026-10-12): 192 of 340 functions lifted. 196 hooks installed in total (4 hand-written). Idle server, 180 s: no crash, 11 lifted functions verified, 0 differences.

With the stack cookie, register preservation and `_ftol2` fixes (2026-10-12): 216 of 340 functions lifted. 220 hooks installed in total. Idle server, 300 s: no crash, 11 lifted functions verified, 0 differences.

An idle server calls only a few functions. More coverage needs players: see the record and replay plan in `docs/plano.md`.

## Stack cookies

`0x5ce18` is `__security_check_cookie` (MSVC `/GS`). It stops the process only when the stack is corrupt. Its failure path calls `GetCurrentProcess`, so the side effect analysis blocked every function with a cookie. The lifter removes the call (`SKIPPED_CALLS` in `lift.py`): the lifted copy has its own stack frame, so the check has no meaning there.

## Registers that a callee keeps

The normal `__cdecl` model says that a call destroys ECX, EDX, ST0 and ST1. The 024d callers know better (see above). The decompiler then shows the kept values as `extraout_EDX` or `extraout_ST1`, and the pseudo-C cannot compile.

`tools/ghidra/FixRegisterPreservation.java` finds the registers that each function writes, with its callees. It adds variants of `__cdecl` to the Ghidra program: `__cdecl_keepC`, `__cdecl_keepD`, `__cdecl_keepF` and their combinations (C = ECX, D = EDX, F = the x87 stack). Each function gets the variant that matches. For C, the variants are `__cdecl`: the lifted hooks keep ECX, EDX and the x87 state.

The same script gives `_ftol2` (`0x5ce90`) a call fixup with its exact effect: it pops ST0 and returns the truncated value in EDX:EAX. The lifter writes it as a C cast to `longlong` (`INLINE_CALLS`).

## Global types

The Ghidra symbol map often keeps the listing type of a global (`undefined4`). The decompiler can use the same global as a `float`. In C, an `undefined4` global that receives `1.0` stores the int 1. `LiftExport.java` therefore uses the type that the decompiler used when it has the same size. This bug made `area_block_addbasicsurface` write `0x00000001` where the original writes `1.0`.

## Verifier: scope rounds

A scope round protects only the pages that the function wrote before, plus a margin of 16 pages on each side. A write outside the scope is not undone before the original runs, so a scope round can show a false difference. The verifier does not count a difference in a scope round. It makes the next verification of the hook a full round, and only a difference in a full round counts.

## x87 intrinsics

`_CIsqrt`, `_CIsin`, `_CIcos`, `_CItan`, `_CIacos`, `_CIatan2` and `_CIpow` take their arguments on the x87 stack (ST0, or ST1 and ST0) and return in ST0. Two steps make them work:

1. `tools/ghidra/FixX87Intrinsics.java` gives them their real signatures in the Ghidra project (run it once, without `-readOnly`). The pseudo-C then shows the arguments: `_CIsqrt((double)(x * x + y * y))`.
2. `src/lifted/x87_intrinsics.c` has a naked thunk for each one. The thunk loads the arguments on the x87 stack and jumps to the routine of the game CRT. The result is bit-for-bit the same as in the original.

## Floats

Lifted code compiles with `/arch:IA32` (x87, like the original). Float returns are compared after rounding to `float` (`SV_HOOK_RET_FLOAT`): the original often rounds before it returns, and Ghidra loses that rounding. Callers store the result as `float`.
