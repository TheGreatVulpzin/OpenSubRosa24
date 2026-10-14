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
| Partial access (`x._4_2_`) | Pointer access with the right size, and `float` when the p-code piece is a float (`LiftExport.java`, `partialAccess`). As `uint`, `local_64 = local_5c._0_4_` converted the bits of a cosine to a number. |
| Calls to other game functions | Through the game address (so hooks and originals both work) |
| CRT and Windows imports | Through the game IAT (`sr_import`), so `rand()`, `FILE*` and the heap are the game ones |
| Absolute addresses shown as numbers (`ptr < 0x46177c`) | The values at relocation sites become `SR_VA(...)` (`relocatedConstants` in `LiftExport.java`, see "Relocated constants") |
| Structs and global arrays (`g_human[i].money`) | The Ghidra project has them (`ApplyTypes.java`, see "Types in the Ghidra project") |

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
| `harness_only` | Lifted, but the function reaches the network, files, time, the heap or another impure import. The live verifier runs a function twice, so the live server keeps the original. The offline harness tests it in a test environment. |
| `side_effect` | Not lifted: the CRT startup code, `dedicatedserverloop` and `getpaths` (written by hand in `src/server/main_loop.c`). |
| `register input/output` | The decompiler shows values passed in registers (non-standard convention). Most cases are gone after `FixRegisterPreservation.java`. The rest are CRT helpers and callees that return a second value in EDX. |
| `int typed as float (denormal literal)` | Ghidra typed an int parameter as float (`param_1 == 1.4013e-45`). |
| `compile_error` | The pseudo-C does not compile. |

## Results

First complete run (2026-10-12): 141 of 340 functions lifted and compiled. Automatic test on an idle server (180 s): no crash, 10 lifted functions called and verified, 0 differences.

With the x87 intrinsics (2026-10-12): 192 of 340 functions lifted. 196 hooks installed in total (4 hand-written). Idle server, 180 s: no crash, 11 lifted functions verified, 0 differences.

With the stack cookie, register preservation and `_ftol2` fixes (2026-10-12): 216 of 340 functions lifted. 220 hooks installed in total. Idle server, 300 s: no crash, 11 lifted functions verified, 0 differences.

With the Ghidra types (2026-10-14): 276 lifted + 39 harness_only. Harness, calls same: `play-20261012` 2917, `20261013-1016-smoke` 1679, `20261013-query` 2213, `20261013-195210` 3652 (no count dropped).

An idle server calls only a few functions. More coverage needs players: see the record and replay plan in `docs/plano.md`.

## Stack cookies

`0x5ce18` is `__security_check_cookie` (MSVC `/GS`). It stops the process only when the stack is corrupt. Its failure path calls `GetCurrentProcess`, so the side effect analysis blocked every function with a cookie. The lifter removes the call (`SKIPPED_CALLS` in `lift.py`): the lifted copy has its own stack frame, so the check has no meaning there.

## Registers that a callee keeps

The normal `__cdecl` model says that a call destroys ECX, EDX, ST0 and ST1. The 024d callers know better (see above). The decompiler then shows the kept values as `extraout_EDX` or `extraout_ST1`, and the pseudo-C cannot compile.

`tools/ghidra/FixRegisterPreservation.java` finds the registers that each function writes, with its callees. It adds variants of `__cdecl` to the Ghidra program: `__cdecl_keepC`, `__cdecl_keepD`, `__cdecl_keepF` and their combinations (C = ECX, D = EDX, F = the x87 stack). Each function gets the variant that matches. For C, the variants are `__cdecl`: the lifted hooks keep ECX, EDX and the x87 state.

The same script gives `_ftol2` (`0x5ce90`) a call fixup with its exact effect: it pops ST0 and returns the truncated value in EDX:EAX. The lifter writes it as a C cast to `longlong` (`INLINE_CALLS`).

## Global types

The Ghidra symbol map often keeps the listing type of a global (`undefined4`). The decompiler can use the same global as a `float`. In C, an `undefined4` global that receives `1.0` stores the int 1. `LiftExport.java` therefore uses the type that the decompiler used when it has the same size. This bug made `area_block_addbasicsurface` write `0x00000001` where the original writes `1.0`.

## Types in the Ghidra project

The lifter reads the types that the Ghidra project has. Better types give better pseudo-C for all functions, so the project holds them and `lift.py` has no text rule for them.

1. `tools/re/gen_types.py` makes `src/include/sr_types.h` from `analysis/structs/names.csv`. Add a known field to the CSV, never to the header.
2. `tools/ghidra/types_json.py` reads the header and writes a JSON file. `tools/ghidra/ApplyTypes.java` puts the structs and the global arrays (`g_human`, `g_object`, `g_objecttype`... with the `g_` prefix) into the project. Run both after each change to the structs or the globals.
3. In `sr_types.h` each struct is a union with `lf_raw[size]`. A byte array that has no field in Ghidra prints as `field_0x5350`. `LiftExport.java` prints it as `lf_raw[0x5350]`, so the C compiles with the same layout.

4. `tools/ghidra/ApplyNames.java` gives the functions their names from `analysis/024d/functions.csv`. It changes only the default names (`FUN_...`) and skips the CRT names (a leading `_`). The names are cosmetic for the lifter, because `lift.py` calls each callee through its address. First run (2026-10-14): 322 functions renamed, and the lifted C has no `FUN_` name left. A `LIFT_PATCHES` text uses the callee name, so update it when a name changes.

The Ghidra project is not versioned. Make a backup before `ApplyTypes.java` or `ApplyNames.java`. The knowledge that must stay is in the CSV, the header and the scripts.

## Relocated constants

`relocatedConstants` (in `LiftExport.java`) replaces all the old text rules for addresses. The lifted code runs the 024d at another base, so each absolute address must be `SR_VA(addr)`. The rule reads the relocation table of the function:

- A constant is relocated when the instruction of its p-code op has a relocation with the same value, or with the value minus a small field offset (`iVar2 + 0x9fdd8a8` is `&DAT_09fdd8a4 + 4`).
- Ghidra can fold the offset into a constant and move it to another instruction (the second store of an inlined `strncpy`, `newspaper_load`). When the instruction has no relocation, the nearest relocated value of the function up to 0x40 bytes below the constant is the base.
- The index of a `PTRADD` is relocated when `index * element size` is a relocated value (`LF_RELOC`).
- A RAM address without a symbol (`ram0x1992d6ed`) becomes a pointer access to `SR_VA()`.
- Each token changes once, so no address gets two relocations. The old regex rules did this twice for the `serversend` tick.

## Pieces of expressions

`expressionPieces` handles Ghidra pieces of a struct area without a field (`g_itemtype[8]._40_4_`, `x.f._0_4_`, `p->f._0_4_`). It finds the start of the postfix chain (names, `[..]`, `.field`, `->field`) and prints `(*(T *)((uchar *)&chain + off))`. `HighGlobal` names (`g_parser`) come from the decompiler symbol, not from a text search.

`bitCast` also reads a whole postfix chain: `(float)g_body[1].unk_0010` is a bit cast of the field load. The CAST op must be the printed cast, and the operand ends where the tokens leave the p-code subtree.

The type of a piece (`pieceType`) is `float` only when the piece is a float. The piece of a CAST is not: in `local_b4[1] = (float)g_objecttype[12]._188_4_;` the CAST output is the float, and the piece keeps the bits of an int. As a `float` piece, `LF_BITS_F` converted the value to `uint` and `setupobjectypes_train` stored `0xffffffff` instead of 0.528.

## Float constants and conversions

- `exactFloat`: Ghidra prints a float constant in a short form (`0.2`, `3.1415927`). In C the short form is a double with another value. `LiftExport.java` prints the exact value of the 4-byte or 8-byte constant as a double literal (`0.20000000298023224`), so the type of the expression does not change. Ghidra can also make a 4-byte constant from a double in `.rdata`: `packangle` divides by the double `6.28318530718` (`FDIV double ptr`), and the p-code constant is the float `6.2831855`. When the function reads no float with that value but reads one double that rounds to it, the double is the value (`collectRdataFloats`).
- `choppedRound`: MSVC converts a float to an int with `FNSTCW`, `OR 0xc00` (rounding control "chop"), `FLDCW`, `FISTP`, and `FLDCW` of the old word. Ghidra prints the `FISTP` as `ROUND(x)` and drops the `FLDCW`. `ROUND` is `rint()`, which rounds to nearest. A `ROUND` from a `FIST`/`FISTP` with a chop `FLDCW` before it becomes `LF_CHOP(x)` (`trunc()`). All 50 `ROUND` of the 024d are of this kind. `serversend` packed the view angle one unit too high (byte 23 of packet 4).

## Float stores into raw bytes

Ghidra prints `X = f;` when a float goes into raw bytes (`undefined4` or `undefined8`). The original copies the bits. In C the assignment converts the number, so 0.42 becomes 0. `LiftExport.java` (`floatStores`) marks the left side: `*(float *)&(X) = f;`, or `double` for 8 bytes.

The rule uses the p-code, not the text:

- A `STORE` through a pointer: the pointed type is raw bytes, and the stored value has a float type.
- An assignment to a variable: the declared type (the symbol type) is raw bytes, and the decompiler uses the variable as a float. Ghidra can declare `undefined8 local_30` and use `local_30` as a `double`.

The first version of this rule was a regex in `lift.py`. It read the `=` of `<=` as an assignment and it marked integer results such as `(int)(longlong)(x)` as floats. The p-code rule fixed `playerbuystock`, `playersellstock`, `playerdeathtax` and the crashes of `createobject`.

## Other bit rules in LiftExport.java

| Rule | Ghidra prints | Problem in C | Output |
| --- | --- | --- | --- |
| `bitCast` | `(int)*pfVar14`, `(float)p[2]` | The cast converts the number. | `(int)LF_BITS_U(*pfVar14)`. The end of the operand comes from the p-code. |
| `floatBitCompares` | `fVar5 != -NAN` (an `INT_NOTEQUAL`) | Every compare with a NaN is unordered, so `!=` is always true. | `LF_BITS_U(fVar5) != 0xffffffffU` |
| `partialAccess` | `local_5c._0_4_` | A `uint` access converts a float piece. | `(*(float *)((uchar *)&local_5c + 0))`. The piece is a float when its p-code piece has a float type, or when a float operation reads it (`(double)local_60._0_4_`, `f < local_40._0_4_`). |
| `pieceVariable` | `local_14` (declared `undefined4`, used as a float) | A read converts the int. | `(*(float *)((uchar *)&local_14 + 0))`. Also for a smaller high variable in an `undefined8` symbol. |

The rules run in this order: `floatStores`, `storeRounding`, `floatGrouping`, `callArgBits`, `floatBitCompares`. A later rule puts its parenthesis outside the earlier ones at the same token. So `lf_f2u((float)(x))` keeps the rounding inside the bit copy.

`FixReturnTypes.java` gives `objecttype_addbond` (0xb0b0) an `int` return and stores its parameters. The `__cdecl_keepF` prototype lists ST0 before EAX as an output, and an `undefined4` return matched ST0. A caller kept 1.0 on the x87 stack across the call, and the pseudo-C read it as the return value. Do not lock a signature that has no stored parameters: every caller then drops its arguments.

## Lint

`python -I tools/lift/lint.py` compiles the lifted C with `/W4` and lists two kinds of warnings by function:

- **An implicit conversion between a float and an int (C4244).** The machine code converts only with explicit instructions (`fild`, `fistp`), and Ghidra prints them as explicit casts. So an implicit conversion is a candidate lifting bug. Check each one in the p-code before you change it.
- **A local read before any write (C4700).** Check the original. `createvehicle` (0x3ccc0) is a bug of the game itself: for `param_1 != 0` it returns `[esp+4]`, a stack slot that it never writes. The lifted code returns another unwritten value. Callers must ignore this return.

The game has bugs of its own (for example the voice index overflow). The lifted code keeps them, because it must do the same as the original.

## Call arguments with the bits of another type

Ghidra prints a call argument with the type that the caller uses. The callee can use another type for the same 4 bytes, and the original pushes the bits unchanged. In C the call converts the number. `LiftExport.java` (`callArgBits`) wraps the argument:

| Parameter | Argument | Output |
| --- | --- | --- |
| `float` | pointer | `lf_p2f(arg)` |
| `float` | constant | `lf_u2f(arg)` (`0x3f8c0000` is 1.09375) |
| `float` | other 4 bytes | `lf_u2f((uint)(arg))` |
| not `float`, not a pointer | `float` | `lf_f2u(arg)` |

The argument type comes from the p-code input of the `CALL`. The parameter type comes from the decompiler prototype of the callee, the same one that `lift.py` reads from the callee header. The stored signature can have no parameters. The regex version did not know the type of an expression such as `pfVar11[1] * fVar1`.

`tools/ghidra/DumpTokens.java` prints the tokens of some functions with their p-code op and types. Use it to check a rule.

## Verifier: scope rounds

A scope round protects only the pages that the function wrote before, plus a margin of 16 pages on each side. A write outside the scope is not undone before the original runs, so a scope round can show a false difference. The verifier does not count a difference in a scope round. It makes the next verification of the hook a full round, and only a difference in a full round counts.

## x87 intrinsics

`_CIsqrt`, `_CIsin`, `_CIcos`, `_CItan`, `_CIacos`, `_CIatan2` and `_CIpow` take their arguments on the x87 stack (ST0, or ST1 and ST0) and return in ST0. Two steps make them work:

1. `tools/ghidra/FixX87Intrinsics.java` gives them their real signatures in the Ghidra project (run it once, without `-readOnly`). The pseudo-C then shows the arguments: `_CIsqrt((double)(x * x + y * y))`.
2. `src/lifted/x87_intrinsics.c` has a naked thunk for each one. The thunk loads the arguments on the x87 stack and jumps to the routine of the game CRT. The result is bit-for-bit the same as in the original.

## Floats

Lifted code compiles with `/arch:IA32` (x87, like the original). Float returns are compared after rounding to `float` (`SV_HOOK_RET_FLOAT`): the original often rounds before it returns, and Ghidra loses that rounding. Callers store the result as `float`.

## Offline differential test (harness)

`src/tests/harness.c` tests the lifted functions without a server and without players.

1. Make a copy of the 024d at a fixed base, for the test server and for the harness:

   ```bash
   python -I tools/re/fixed_base.py original/blackhouser/subrosadedicated024d.exe build/auto-server/subrosadedicated024d.exe
   python -I tools/re/fixed_base.py original/blackhouser/subrosadedicated024d.exe build/hook/024d_fixed.exe
   ```

   Both copies load at 0x20000000 with ASLR off. A snapshot of `.data` from the server then fits the harness with no pointer fixes.

2. Record real inputs. In `build/auto-server/subvulpes.ini`, set `record_args=1` and `snapshot_after=150`, then run the server (`tools\autotest.ps1 -Seconds 200`). The thunks of the lifted hooks write `sv_args.txt` (up to 24 different calls per function, `hook/record.c`). After 150 seconds the DLL writes `sv_snapshot.bin` (the non-zero pages of `.data`).

3. Run the harness:

   ```bat
   build\hook\harness.exe build\hook\024d_fixed.exe build\auto-server\sv_snapshot.bin build\auto-server\sv_args.txt [name filter]
   ```

   It loads the 024d as a library, resolves its imports, fills `.data` from the snapshot and runs each recorded call twice: the lifted version, then the original (the verifier undoes the writes of the first run). It compares the `.data` writes, the return value and the memory behind pointer arguments. A full run takes a few minutes. The details of each difference go to `build/hook/subvulpes.log`.

## Float precision

The 024d runs the x87 with 53-bit precision (control word `0x27F`). The original keeps intermediate values in x87 registers, and the lifted C code rounds them to `float` at other places. Some float results therefore differ in the last bits.

The verifier accepts two normal floats with the same sign that are at most `verify_float_ulps` steps apart (default 4, 0 = exact). It counts these words apart, and the log says "floats differ in the last bits only".

Two rules in `LiftExport.java` put the rounding points of the original back into the C:

- `storeRounding`: the original rounds a value to `float` only when it stores it (`fstp dword ptr [esp+10h]`). The decompiler p-code has only 4-byte float ops, so it does not show these stores. The rule follows the x87 stack in the machine code after each float op. When the result goes to a `float` store before another x87 instruction uses it, the rule prints `(float)(...)` around the op. A Ghidra listing shows `faddp st(1),st` as `FADDP` without operands.
- `floatGrouping`: Ghidra prints `FLOAT_ADD(x, FLOAT_ADD(y, z))` as `x + y + z`, which C reads as `(x + y) + z`. Float math is not associative, so the rule puts the parentheses back. The same applies to `*` and `/`.

These two rules made `createlandscape` (a 2049 x 2049 height map) bit-exact. Known functions that still have precision differences (the verifier disables their hooks, so the original runs):

| Function | Difference |
| --- | --- |
| `buildsomeshit` | about 6000 bytes, the error grows through the computation |

`/fp:fast` makes the differences larger.

### Probes

To find where a lifted function starts to differ, compare its intermediate values with the original:

```bash
HARNESS_PROBE=2fe39,2fd14:3c build/hook/harness.exe <exe> <snapshot> <args> human_calcarmtorque
```

Each probe puts an `INT3` on an instruction of the original (RVA). It prints ST0, ST1, EAX, EBX, ESI, EDI and EBP. With `:off`, it also prints the float at `[esp+off]`. With `:off*`, `[esp+off]` is a pointer, and the probe prints the 3 floats there (a vector argument of a call). Put `printf()` calls at the same places in the lifted C (in `src/lifted/lifted_024d.c`, then lift again to remove them). `tools/ghidra/DumpTokens.java <out> <rva>` prints the decompiler tokens with the p-code op, its address and its input types. `insn:<rva>` prints the instructions as the Ghidra listing shows them.

### Watchpoint

To find which instruction writes a different value, use the RVA from the difference in `build/hook/subvulpes.log`:

```bash
HARNESS_WATCH=e2c8db0 build/hook/harness.exe <exe> <snapshot> <args> building_buildfloorplan
```

The harness puts a hardware data breakpoint (DR0, 4 bytes) on the address. It prints each write with the new value and the place after the instruction: an RVA in the 024d (the original) or an address in the harness (the lifted code). The verifier also writes the address when it undoes the first run.

## Side-effect functions

The harness tests them in a test environment (`src/platform/plat_test.c`). See `docs/side-effects.md`.

The lifter also needs these rules for them:

- `lift_os.h`: the CRT, KERNEL32 and WinSock types (`FILE`, `LARGE_INTEGER`, `sockaddr`, `fd_set`...) with the MSVC 2010 layout. Only `lifted_024d.c` includes it.
- WinSock imports by ordinal (`WSOCK_ORDINALS`, `LF_IMPORT_ORD`).
- A global that holds a function address (`(*DAT_095022a4)(...)`, the `fread` pointer) gets the type `code *`.
- An inlined `strncpy` stores 4 bytes at a time, and Ghidra adds the offset to the address (`iVar2 + 0x9fdd8a8`). `relocatedConstants` handles it (see "Relocated constants").
- `ram0x1992d6ed`: a 4-byte store into the middle of another global.

## Delinker test (Phase 4)

`tools/ghidra/delink.sh <first VA> <last VA> <output.obj>` exports a range of the 024d as a COFF object file with `ghidra-delinker-extension` (`DelinkProgram.java` runs the "Relocation table synthesizer" analyzer on the range). The exporter name is `COFF relocatable object`.

Test on `init_genrand` (0x40ad30 to 0x40ad6a, 59 bytes): `dumpbin /symbols /relocations /disasm` shows one `.text` section of 0x3B bytes, four `DIR32` relocations and three undefined externals (`DAT_00460cd0`, `DAT_00460dc0`, `DAT_0046177c`). The symbols carry the Ghidra names (`FUN_0040ad30`, `DAT_...`), not the names of the CSV files. Rename them in the project before a real export.
