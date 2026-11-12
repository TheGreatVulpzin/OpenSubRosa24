# Client decomp (Phase 7)

Target: `original/client/subrosa.exe`. It is the 0.24b client of 2015-08-02, built with MSVC 2010. Ghidra name of the program: `client_win`.

## Name sources

| Source | Use |
| --- | --- |
| `original/client/subrosa.x64` | ELF with 2757 function symbols. Main source of names. |
| 0.24b Mach-O i386 (`mac_i386`) | Same version as the exe. Second source. |
| 0.25b Mach-O i386 (`mac025_i386`) | Not downloaded. The 0.24b Mach-O is closer to this exe, so it adds little. |
| Machine code | GLee names, see below. |

`tools/re/name_all.sh` runs the matcher and the GLee step. The output is `analysis/client_win/functions.csv`.

## Files in `analysis/client_win/`

| File | Content |
| --- | --- |
| `functions.csv` | All 2907 functions. Columns: `rva,size,name,source,confidence,evidence`. Generated. |
| `glee_names.csv` | Names from `tools/re/name_glee.py`. Generated. Merged as manual names. |
| `globals.csv` | 49 global candidates from `map_globals.py`. Generated. See Globals below. |
| `globals_checked.csv` | Status of each candidate, from `tools/re/check_globals.py`. Generated. |
| `globals_review.csv` | Candidates reviewed by hand in Ghidra, with evidence. Input of `check_globals.py`. |
| `manual_globals.csv` | The 1595 GLee function pointers, named `GLeeFuncPtr_<function>`. Generated. |
| `manual_names.csv`, `data_globals.csv`, `params.csv`, `locals.csv` | Same format as the files in `analysis/024d/`. Only `manual_names.csv` has a row (`__GLeeGetProcAddress`). |

## GLee

The client links GLee statically. About 1750 of the 2907 functions belong to it. They have two fixed shapes:

- **Link function** (`__GLeeLink_<extension>`). It calls `GetProcAddress("glXxx")` and stores the result in a pointer global. `tools/re/name_glee.py` reads the string and the global from the machine code. It gives the function the name of the Linux function that loads the same strings.
- **Stub** (`GLee_Lazy_glXxx`). It calls a check function for the extension, then calls through the pointer global. The pointer name gives the stub name.

Not named: 3 link functions with no Linux match (`0x1e370`, `0x1e910`, `0x20fd0`) and the wgl link functions, which exist only on Windows.

## Globals

`tools/re/map_globals.py` gives 49 candidates in `globals.csv`. They are big arrays such as `human`, `player`, `building`, `city` and `texture`. The ELF and the Mach-O have only function symbols in the features, so each vote comes from a named function that uses the global.

`tools/re/check_globals.py` checks each candidate against the code of the exe and writes `globals_checked.csv`. Two kinds of evidence:

- **Layout:** the candidate ends exactly where the next one starts. The sizes come from the reference binary, so an exact chain is strong.
- **Access:** the code has many 32-bit addresses inside the range of the candidate.

| Status | Count | Meaning |
| --- | --- | --- |
| confirmed | 14 | Exact layout chain (`area_sectormodel`, `timerlist`, `speex`, `area_block`, `hack_trainintersection`, `bsdsocket`) or reviewed in Ghidra. |
| likely | 29 | Strong votes, the size fits, many accesses. Or reviewed in Ghidra with a weaker link to the name. |
| unverified | 1 | Strong votes, the size fits, few accesses: `alcapturedevice`. |
| doubtful | 4 | Weak votes, or no reference to the base address in Ghidra: `failure_reason`, `buttonmesh`, `model_human`, `sphere`. |
| rejected | 1 | `lightwavelength` is the security cookie of the CRT. |

`globals_review.csv` holds the rows reviewed by hand in Ghidra (xrefs to the global, then the names of the functions that use it). Each row has its evidence. `check_globals.py --review` merges it by name. A reviewed row applies when the voted address moved by at most 16 bytes, which happens when new function names change the votes. The function names in that evidence come from the matcher, so they are evidence too, not proof. The 24 `likely` rows that no one reviewed in Ghidra stay `likely`.

`area_block` and `area_sector` have the right start, but the size from the reference binary runs into the next candidate. The real size needs a check before a size goes into a CSV.

No candidate is in `manual_globals.csv` yet. That file needs the element count, and the candidates give only the byte size.

`tools/re/map_regions.py` works only for the 024d (fixed paths). A client version needs new paths first.

A cycle of matcher and globals, as for the 024d, gave 9 new names and 6 renames. Three of the six renames lowered the confidence. The cycle is not in `name_all.sh`.

## Lift (step 7.3)

The lifter runs on the client with `--target client_win`. Its output goes to `build/lift/client_win/` (not versioned), so a client run never changes a file of the server.

Steps, once per Ghidra project. Make a backup of the project first. Run the scripts without `-readOnly`:

1. `ApplyNames.java analysis/client_win/functions.csv`
2. `FixX87Intrinsics.java`, `FixRegisterPreservation.java`
3. `FixCookieCheck.java`, `CommitParams.java`, `ApplyReturns.java analysis/client_win/returns.csv`, `FixKeepFReturns.java`

Do not run `FixReturnTypes.java` on the client. It has fixed RVAs of the 024d.

Then export and lift:

```bash
source instrumentation/ghidra/env.sh
"$HEADLESS" "$PROJECT" subvulpes -process subrosa.exe -noanalysis -readOnly     -scriptPath "$ROOT/instrumentation/ghidra" -postScript LiftExport.java "$ROOT/build/lift/client_win.jsonl"
python -I instrumentation/lift/lift.py --target client_win --compile
```

Two scripts matter most:

- `FixCookieCheck.java`. A function that ends with `__security_check_cookie` looks like a void function to the decompiler, so its callers read a value from a void function.
- `ApplyReturns.java`. Functions that only pass on a call (`call X; ret`) are typed void when the decompiler does not know the return of `X`. `returns.csv` lists 24 of them with the evidence. The biggest is `__GLeeGetProcAddress` with 1598 callers.

Result (2026-11-03): 2907 functions. 1015 lifted and compiled (295 of them only for the harness). 237 reach a side effect (OpenGL, heap, files, network). 21 do not compile. 1634 are blocked, and 1600 of those are GLee stubs that call through a pointer. The GLee code will get a platform layer, so it needs no lift.

What the target changes in `lift.py` (`TARGETS`): the files, the `.text` range, the cookie function, the pure imports (the client calls `sqrt`, `sin`, `pow` and similar as plain imports), and the extra header `src/lifted/lift_os_client.h` (Win32 types).

Open errors:

- 13 functions: `C2197`, too many arguments in a call through a pointer. Among them `GLeeInit`, `setupaudio`, `shutdownaudio` and `loadwav`.
- 8 functions: syntax errors and void values.

Next: the side-effect list, then the recorder for the client (step 7.4).

## Plan for 7.4 (hook DLL and recordings of the client)

The server needs these parts. Each one has a client copy to make:

| Part | Server file | Client work |
| --- | --- | --- |
| Proxy `wsock32.dll` | `instrumentation/hook/proxy.c` | Same forwards. Change the identity check: the PE time stamp and the image size of `subrosa.exe`. |
| Hook table | `instrumentation/hook/hooks_lifted.c` | Made by `lift.py --target client_win` in `build/lift/client_win/`. Use it in a second CMake target. |
| Lifted code | `src/lifted/lifted_024d.c` | Made by the same run (`lifted_client.c`). |
| Recorder | `instrumentation/hook/record.c`, `hooks_record.c` | Records the arguments and the memory of the side-effect functions. |
| Harness | `instrumentation/tests/harness.c` | Needs the fixed image of `subrosa.exe` and test stubs for OpenGL, OpenAL and SDL (`instrumentation/platform/plat_test.c` has the server ones). |

The fan `wsock32.dll` in `original/client/` is a proxy of the same kind. Our DLL replaces it for the tests, so keep a copy of the fan DLL.

Order of work:

1. Second CMake target `cl_wsock32` with the client proxy and the client tables. It must build and load in the client without hooks first (forward only).
   Done for the build: `instrumentation/client/cl_proxy.c` and `wsock32.def` give `build/hook/client/wsock32.dll` with the 47 exports. The forward list is in `instrumentation/hook/wsock_forwards.h`, shared with the server proxy. The DLL writes `subvulpes_client.log` next to the executable. A load test in the client is still open: copy the DLL next to `subrosa.exe` (keep the fan `wsock32.dll`) and start the game.
2. Turn on the verify mode for a few small pure functions. Run the client to the main menu and check the log.
3. Record a session: start the client, go to the menu and join a test server. The user plays this part.
4. Port the harness: image loader for the client, the stubs, and a target file `instrumentation/lift/targets/client_win.json`.
5. Run the gate. Every function that reaches a side effect stays on the original code until the harness can replay it.

Risks: the client draws with OpenGL on every frame, so the recorder sees a lot of calls. The 237 side-effect functions (mostly OpenGL and heap) need a stub plan before step 4.

### Call counter (step 7.4)

`instrumentation/client/cl_count.c` counts calls and writes no arguments. It is in `cl_wsock32` when `build/lift/client_win/cl_count_table.c` exists. Make that file with `python -I instrumentation/client/gen_count_table.py` after each lift.

Switch it on with `subvulpes.ini`, section `[hooks]`, `count=1`. The keys `count_game`, `count_stub`, `count_gl` turn one group off. `game_from` and `game_to` limit the game hooks to a range, for a bisect. The files next to `subrosa.exe` are `subvulpes_counts.log` (one line per second) and `subvulpes_counts.csv` (totals per function, every 5 seconds). `subvulpes.log` has the first exceptions with the offset in the executable.

Groups: the game hooks (the same functions as the lifter hook tables), the GLee stubs, and the OPENGL32.dll imports (through the import address table).

The client loads at a moved base (`0x007f0000`). The counter adds the delta to the absolute addresses of each prologue.

Rule found with the counter: a hook must not cover an instruction that another branch of the function targets. `findstring` has a loop that jumps back to its offset 3, inside the 8 bytes of the patch. The client crashed. `LiftExport.java` now marks such a prologue as not hookable (10 client functions, among them `findstring`). The 024d export has not run again with this rule.

Measured on the main menu (2026-11-03, 25 seconds):

| Group | Calls |
| --- | --- |
| Game functions, first 4 seconds (texture decode and noise: `noise2d`, `paeth`, `zhuffman_decode`) | about 14 million |
| Game functions, after that | about 40 000 per second, 260 different functions |
| OpenGL imports | about 550 000 per second (`glVertex2fv`, `glTexCoord2fv`, `glColor4f` in immediate mode) |
| GLee stubs | 0 (the menu uses no extension function) |

Result for the recorder: never record OpenGL calls one by one. Record the game functions with a cap per function.

### Recorder (step 7.4)

`cl_wsock32` also holds the lifted client code, the hook tables of the lifter, the verifier and the recorder of the server (`verify.c`, `record.c`, `record_imports.c`). `instrumentation/client/cl_hooks.c` installs them. They need `src/lifted/client/lifted_client.c` (versioned and frozen, see CLAUDE.md), so run `lift.py --target client_win` only when the lifted C is missing.

Switch it on with `subvulpes.ini`, section `[hooks]`:

```
hooks=1
lifted=1
original_only=1
verify=0
record_args=1
record_imports=1
snapshot_after=600
```

The files next to `subrosa.exe` have the same format as the server ones: `sv_args.txt` and `sv_snapshot.bin`. The harness port will read them.

First test on the main menu (2026-11-03, 50 seconds, `snapshot_after=20`): 1031 hooks installed, 295 recorded calls with 8632 import events, `sv_args.txt` 0.3 MB, `sv_snapshot.bin` 125 MB. The rotation of `record.c` removes a hook after enough calls, so the file stays small.

Known gaps:

- `record_imports.c` wraps the imports of the server. The log shows 7 errors for `_CIsqrt` and similar names that the client does not import. The imports of the client (OPENGL32, OpenAL32, SDL2, steam_api, speex) have no wrapper yet. A side-effect function replays with the events of the wrapped imports only.
- `verify.c` names a `.data` address with the global table of the server. The names in the log are wrong for the client.

### Harness for the client (step 7.4)

`harness_client` is the harness of the server, built with the client sources (`CL_HARNESS`). It needs the fixed copy of the client and clean DLLs, so no proxy loads inside the harness:

```bash
python -I tools/re/fixed_base.py original/client/subrosa.exe build/hook/subrosa_fixed.exe
# Record with the fixed copy: copy it next to the proxy as subrosa_fixed.exe and start that.
# build/client_deps holds SDL2, OpenAL32, OpenAL32_real, wrap_oal, libspeex, libspeexdsp, steam_api, Steam (copies of original/client, no wsock32.dll).
export PATH="$PWD/build/client_deps:$PATH" HARNESS_GAME_DIR=<client folder> HARNESS_ENV_DIR=build/harness_env_client
build/hook/harness_client.exe build/hook/subrosa_fixed.exe sv_snapshot.bin sv_args.txt [filter]
```

The snapshot fits only when the recording process ran the fixed copy (base `0x20000000`). A recording of the normal exe has another base.

First result (2026-11-03, recording of the main menu, 295 calls): 78 functions tested, 120 calls same, 0 different, 0 lifted crashed, 161 skipped. The run takes 52 seconds.

Changes the client needed in shared code:

- `plat_test.c` has a `realloc` stub. The server never imported `realloc`. Without it the call reached the real heap and the process died with `0xC0000374`.
- `LiftExport.java` ignores relocations of type 0 (ABSOLUTE). They pad a relocation block. A function that starts at a page boundary got a relocation at offset 0, so its hook never matched.
- `harness.c` has no hand-written function table for the client, and logs the first exceptions outside the guarded runs.

Next: the gate target `instrumentation/lift/targets/client_win.json`, and a longer recording with a match, to reach more functions than the main menu.

### Platform layer for the side-effect functions (step 7.4)

The lifter lifts a side-effect function of the game as "harness only" when the harness can run it. The live client keeps the original code and records the call. The harness runs both versions in the test environment of `plat_test.c`, with stubs for the imports.

Parts:

- `analysis/client_win/imports.csv` has the prototype of 158 imports (OPENGL32, OpenAL32, SDL2, speex, steam_api). Ghidra does not know them. Without a prototype the decompiler drops the arguments of each call and the lift fails with `C2197`. Apply the file with `ApplyImports.java` (no `-readOnly`). The prototype is the last field and keeps its commas.
- `instrumentation/client/gen_plat_stubs.py` makes `build/lift/client_win/plat_client_stubs.inc` from the same file. Each stub has the exact signature of the import and gives fixed results (ids from a counter, empty strings, no events, no joystick). `plat_test.c` includes the file only with `CL_HARNESS`.
- A stub writes what the call did into the environment log. OpenGL calls and the vector calls of OpenAL go into one hash per run and the log keeps the count and the hash. Every other call writes one line with its scalar arguments. A pointer argument is not compared (the stack addresses differ between the two runs), except for the vector calls listed in `PTR_DWORDS`.
- The lifter has two limits per target: `crt_start` (the first RVA of the CRT) and `record_skip` (functions that run a frame loop until the player leaves: `main`, the game loop, the menus, the editors). A caller of such a loop is also left out (`skip_callers`).

Order after a change of names or types: `ApplyImports.java`, `LiftExport.java`, `lift.py --target client_win --compile`, `gen_count_table.py`, `gen_plat_stubs.py`, `tools\build_hook.bat`.

Result (2026-11-03, menu recording): 718 lifted, 471 harness only, 34 side effect (the loops and their callers), 12 compile errors. The harness tests 84 functions: 131 calls same, 0 different, 162 skipped.

Two name errors of the matcher showed in the CRT area (`0x10c000` and up): `0x10c650` is `_ftol2` (it takes ST0), and `0x10c6fb` is `__security_check_cookie`. The matcher names in that area are not reliable. `FixRegisterPreservation.java` and `FixCookieCheck.java` take the address as a script argument.

### First recording of a play session (2026-11-03)

Recording `client-play-20261103-1229` (14 minutes, snapshot at 180 s with `snapshot_after=180`, 1286 recorded calls of 254 functions). Run the harness with `HARNESS_RESET=1`: without it the harness dies in a later function after earlier functions left their state (the heap of the process). The reset run takes 30 minutes for this snapshot (355 MB).

Result: 216 functions tested, 840 calls same, 91 different, 13 lifted crashed, 155 skipped.

The differences are in the drawing functions (`drawdisplay`, `drawcircle`, `drawplayerindicator`, `drawplayerinfo`, `drawhud`, `drawhelp`, `drawmap`, `drawmenuitems`, the model and sector render functions). Most of them are float differences in the last bits. `drawdisplay` and `drawplayerinfo` also show another number of OpenGL calls in the lifted run.

`HARNESS_GLTRACE=1` writes every OpenGL call into the environment log. With `HARNESS_DEBUG=1` the first differing call shows. In `drawcircle` (RVA `0x6d860`, call 1119) the first different call is `glVertex2fv`: x is equal, y is `0x3d9dc980` in the lifted run and `0x3d9dc971` in the original, 15 steps apart. y is `param_2 - (float)cos(a) * param_3`, a subtraction of two close numbers, so a small rounding difference shows large.

Checked so far: the control word of the snapshot is `0x27f` (53 bit), as in the harness. The decompiler has a real `FLOAT2FLOAT` p-code op right after the call of `sin` (address `0x46d8d6`), so the `(float)` of the result is a real store in the original. The open question is where the original rounds the product `(float)cos * param_3` and what MSVC does for the same expression in the lifted C. Compare the instructions with `DumpTokens.java` (`insn:<rva>`) and probe with `HARNESS_PROBE`.

### Float rule: call result narrowing (2026-11-03)

The decompiler shortens the double result of a call (`sin`, `cos`) to float when the expression ends in a float store, and prints `(float)dVar1`. The machine code has no store there: `call sin; fmul dword ptr [x]` multiplies the double. `LiftExport.java` drops that cast (`dropCallNarrowing`) when the FLOAT2FLOAT has a call result as input and sits at the address of a call. A real rounding is an `FST` or `FSTP` of a dword, and `storeRounding` prints it.

After the rule, `drawcircle`, `drawplayerindicator`, `drawcircleindicator` and `createcashmodel` are the same as the original. The re-export of the 024d gives the same lifted C and the same hook tables, so the rule changes nothing for the server.

Open differences on the play recording (8 functions): `drawdisplay`, `drawplayerinfo`, `drawhelp`, `drawhud`, `drawmap`, `drawmenuitems` (the first different call is a `glColor4f` with other arguments, so a color argument or a parameter type differs) and `setupmodel`, `copymodel` (the lifted run frees a block of the recorded heap where the original frees a pool block).

### Compile errors of the client lift: 12 to 5 (2026-11-03)

- `ApplyReturns.java` also sets a `float` return (the result in ST0), for a function that ends with a call that returns a float. `returns.csv` has `pointdistanceline` (float) and `scenario_armhuman` (int). Their callers read a value from a function typed `void`.
- `LiftExport.java` adds `;` after a label at the end of a block (`LAB_xxx:` before `}`), which MSVC rejects (C2143). It does this when it writes the `}`, from the text already written.
- After these two changes `humansimulation`, `area_buildsectormodel`, `soundsimulation`, `calculateplayervoice`, `human_itemsimulation`, `scenario_createchase` and `spritetimetolive` compile. The lift has 723 lifted and 473 harness only functions.

The 5 left: `dedicatedserverloop` (a server function in the client binary), `launchwebpage` (the identifier `INT`), `landscape_buildsectorlods` (a dereference of `undefined4`) and two CRT functions that the standalone client replaces.

The play recording was made before these functions had a hook, so it has no call of `humansimulation` and most of the others. A new recording with the current hook tables is needed to test them.

`area_buildsectormodel` is the same as the original in 4 calls and differs in 20. The first difference is a `free` of another block (the lifted run frees a block of the recorded heap, the original frees a block of the pool), as in `setupmodel` and `copymodel`.

### Wrong parameter count: drawtext (2026-11-03)

The harness showed `glColor4f` calls with garbage arguments in `drawdisplay`, `drawhelp`, `drawhud`, `drawmap` and `drawmenuitems`. All of them call `drawtext` (RVA `0x106b30`). Ghidra gave it one parameter. The code reads `[ebp+0xc]` to `[ebp+0x28]` and starts a variable list at `[ebp+0x2c]`, and the callers push 9 words (138 calls), 10 words (72 calls) and more. The lifted caller passed one argument, so the original function read the rest from the stack.

How to find such a function: count the words that the callers push (`add esp, N` after each call) and compare with the parameters of the prototype. `analysis/client_win/signatures.csv` gives the full prototype of a function, with `...` for a variable list, and `ApplySignatures.java` applies it (no `-readOnly`). `lift.py` keeps the `...` last in the cast and adds no extra parameters after it.

Result: `drawhelp`, `drawhud`, `drawmap`, `drawmenuitems`, `generatetexture_billboard` and `generatetexture_newspaper` are the same as the original (43 calls). Other functions can have a wrong parameter count: a check of all functions against their callers is a good next step.

### State of the harness on the second play recording (2026-11-03)

Recording `client-play2-20261103-2019` (25 minutes, snapshot at 240 s, 1422 calls of 268 functions). Run with `HARNESS_RESET=1` (about 30 minutes).

| Step | Different | Same | Crashed |
| --- | --- | --- | --- |
| First run, before the fixes | 91 | 840 | 13 |
| After the float rule and the compile fixes | 81 | 1035 | 12 |
| After the `drawtext` signature | 31 | 1085 | 12 |

232 functions tested, 126 calls skipped. `humansimulation`, `human_itemsimulation`, `calculateplayervoice` and `scenario_createchase` have no call in this recording, and none in the later recordings, which include a hosted game and the two demos. Their callers (`lf_85b70`, `lf_d4700`, `lf_ddc00`, `lf_deb20`) never ran. They are paths of a local server that the client does not use. Synthetic calls test them (2026-11-06, next section).

### Synthetic calls (2026-11-06)

Three scripts in `instrumentation/client/` write calls and states for the functions that no recording reaches. They use the snapshot of `client-play2-20261103-2019`. The record layout of humans and objects is the same as in the server (`synth_humans.py` of the server), with other base addresses. A case snapshot is about 350 MB: the scripts delete it after each run, and the work folder can be on another drive (`E:/sv_synth_tmp` here, because the C drive was full). Use the environment of the gate.

- `synth_calls.py`: calls with plain integer arguments on the state of the snapshot.
- `synth_humans.py`: one live human at a time (the snapshot holds humans that the client never simulates, and the original crashes on two of them). `situations:<human>` sets on foot, walk modes, crouch, jump, health 20, health 5, dead and despawn. `cars:<human>` sets drive, exit, die in a car and enter.
- `synth_voice.py`: states for `calculateplayervoice` (voice off, a human slot, an item slot, a muted player) and `scenario_createchase` (limit 0, 1, 5).

Results: `humansimulation` is the same in 4 of the 6 live humans (the original crashes on the other 2) and in all cases of `situations` (9) and 4 of 5 cars. `human_itemsimulation` is the same in all of them. `calculateplayervoice` is the same in all 4 states and in 8 calls with the state of the snapshot. `scenario_createchase` is the same with limit 1 and 5. With limit 0 the memory is the same and the return value is different: the original returns the leftover EAX (`cookie xor ebp`) when the function does nothing. This is not a lifter difference, and it is not in the table of known differences, because that table would hide a real difference of the main path.

Not covered: the case "enter a vehicle" (the original crashes in that state) and the paths that need a state that no case builds (items in hands, phones, fights).

The 31 open differences of that run were mostly an artifact of the verifier. A scope round protects only the pages that earlier calls wrote. A page that only the lifted run writes is not undone, so the original run starts from the state that the lifted run left (the first different event was a `free` of another block). The harness of the client now sets `verify_full_every=1` (`SV_FULL_EVERY_DEFAULT` in `CMakeLists.txt`): every round is a full round. A full round is slower (about 150 ms per call instead of 12 ms) and finds differences that a scope round missed.

With full rounds: 232 functions, 998 calls same, 116 different, 11 crashed. `area_buildsectormodel`, `setupmodel`, `copymodel` and `checksimulation` are the same as the original. The open difference is `area_interior_buildsectormodel` (4 of 24 calls, 12 bytes of `.data` differ) and the functions that call it: `lf_d7850`, `render_sector_interior`, `render_sector_alpha`, `render_sector_interior_shadow2` and `rendermodelshadow` (24 calls each). In `render_sector_interior` the bytes that differ are in a global at RVA `0x13346d0`.

Crashes: `building_buildfloorplan`, `getlanipaddress`, `steam_getid`, `lf_10c170`. All four are fixed (2026-11-05):

- `building_buildfloorplan` had a raw address in its lifted C. `LiftExport.java` now matches a constant of `0x400000` or more to a relocation up to `0x100` bytes away.
- `steam_getid` and `lf_10c170` call methods of the Steam class block (`0x10be80` to `0x10c1b0`). Ghidra had dropped the `this` pointer, so the lifted C called them with a garbage ECX. `signatures.csv` gives the 12 methods the `__thiscall` prototype. `lift.py` does not lift a `__thiscall` definition, so these methods stay original code (`lf_10c170` is now blocked). A call to a method with stack arguments passes the unused EDX slot in the `#define` of the callee. A call to a method with no stack argument needs no slot.
- `getlanipaddress` calls `gethostname`, which the client imports by ordinal 57. `WSOCK_ORDINALS` in `lift.py` and the stub table (`BY_ORDINAL` in `gen_plat_stubs.py`) know it. The stub returns -1, as the real call does when Winsock is not started.

The difference of `area_interior_buildsectormodel` was the profiler of the game, not the lifter. The function stores the CPU cycle counter (`RDTSC`, the function at `0xfafc0`) in a record of the array `timer`: records of `0x58` bytes from RVA `0x1332e80`, the time at `+0x40`. Each run writes another value. The harness of the client ignores `0x1332ec0` for `0x5800` bytes. Before, it ignored the range of the server (`0x14043e0`), which is other memory in the client.

### Gate of the client (2026-11-05)

`instrumentation/lift/targets/client_win.json` is the gate target of the client. Run it with:

```bash
python -I instrumentation/client/cap_calls.py build/recordings/<recording>      # sv_args.min.txt, 2 calls per function
python -I instrumentation/lift/gate.py --target client_win --jobs 2             # compare with the baseline
python -I instrumentation/lift/gate.py --target client_win --jobs 2 --update    # write the baseline
```

The target sets `HARNESS_GAME_DIR` and puts `build/client_deps` first on the `PATH` (`env` and `path_prepend`, which `gate.py` reads), and runs in reset mode. The recordings are folders of `build/recordings` (not versioned). The first baseline (3 recordings, 33 minutes with 2 jobs): the menu recording 85 functions and 85 calls same, play 1 218 functions and 265 calls same, play 2 217 functions and 260 calls same. The gate fails when a count of "calls same" drops. Baseline of 2026-11-05 after the fixes above: menu 85 functions and 85 calls same, play 1 217 functions and 279 calls same, play 2 216 functions and 275 calls same. No function has a difference or a lifted crash in the two play recordings.

`instrumentation/client/pipeline.sh` runs the whole cycle: `--export` (Ghidra), lift, tables, build, gate (`--full`, `--update`, `--no-gate`). `--changed` uses `lifted_prev`, the copy of the lifted C that `--update` writes.

`instrumentation/client/cap_calls.py` keeps the first N calls of each function. `min_calls.py` of the server picks calls by code path, and it needs the block file of the binary: the client has none yet.

## Standalone client, level 1 (step 7.5)

Level 1 runs the lifted C in an own process, without `subrosa.exe`. It needs no rewrite. The lifted C is the game code.

Build and run:

```bash
cmd //c "tools\\build_client.bat"
cd build/client_test && PATH="$PWD/../client_deps:$PATH" ../hook/opensubrosa24_client.exe --run
```

`build_client.bat` makes `build/client_image/image_client.c` (`gen_image.py --client`: `.rdata`, the initial `.data` and the relocations, not versioned) and `client_gl.c`. It builds `build/hook/opensubrosa24_client.exe`. Run `instrumentation/client/pipeline.sh --no-gate` first: it makes the lifted C.

`src/standalone/client_main.c` reserves the image region (`0x2a7ff000` bytes), copies the tables, sets the x87 precision to 53 bits and calls `main` (RVA `0x10ae00`) through `sr_fn`. `sr_fn` finds the C function of an image address in `lf_table`. `sr_import` loads the DLL of the game with `LoadLibrary`. The CRT of the game (`MSVCR100.dll`) still comes from the DLL. A later step links it statically, as the server does.

Changes to the lifter, all only for the target `client_win` (`lift_loops`):

- The 15 frame loops (`main`, `gameloop`, the menus, the editors) are lifted. They get no hook and no harness call (status `loop`).
- Functions with a prologue that a hook cannot patch are lifted. They get no hook (status `unhookable`).
- `checkbigendian` stores the addresses of `fread` and `fwrite`. The lifter resolves them as imports (`EXREF_IMPORTS`).
- A method with `__thiscall` is lifted as a `__fastcall` with an unused EDX slot, as its callers call it. The table declares it with the same argument size, so the symbol name matches.
- A function with a variable list (`drawtext`, `createmenubutton`, `FUN_0040ac50`) is lifted when `LIFT_PATCHES` gives the address of the first list argument (`&last_named_parameter + 1`). The signature comes from `signatures.csv`.
- A function used as a value (the `qsort` comparator, the callbacks of a table) is the address of the function. The old macro read the first byte of its code.
- The exporter relocates a constant that is far from the table base (up to 128 KB), only for the client.
- `lift_gl_client.h` and table flag 2 (`standalone_only`): functions that call OpenGL through the slots. The harness skips them, the client runs them.

OpenGL: the game calls 32 extension functions through pointer slots (`PTR_GLee_Lazy_glXxx`). Without a type, the decompiler drops the arguments. `analysis/client_win/gl_slots.csv` gives the prototype of each slot. `ApplyGlSlots.java` types the slots in Ghidra (no `-readOnly`), then `LiftExport.java` runs again. `gen_gl_slots.py` writes the function types for the lifted C (`src/lifted/lift_gl_client.h`) and the fallback functions that fill the slots (`client_gl.c`). `GLeeInit` is lifted, so it fills the slots with the real functions at run time.

Result on 2026-11-06: **the client starts and stays in its main loop.** The process opens the window "Sub Rosa", answers messages and uses about 800 MB. Nobody checked the picture, the mouse, the keyboard or the sound yet. The gate stays green (menu 85, play 285, play2 280 calls same). Debug aids of `client_main.c`: a crash prints the exception, the address and the return addresses (resolve them with `build/hook/opensubrosa24_client.map`), and `SR_TRACE=1` prints every call that goes through `sr_fn`.

Still without C: `joinmenu`, `getinputs`, `playermenu_building_buy`, `debug_rendercylinder`, `pbitreadnego` (register input), `load_jpeg_image` (stack array), `clamp` (odd concat), `launchwebpage` and `landscape_buildsectorlods` (compile errors). The client calls them only in menus that the first start does not open, so each one shows up as "no C function for RVA" when a test reaches it. Update 2026-11-09: `launchwebpage` is now handled by `sr_open_url` of the platform layer, and `landscape_buildsectorlods` has C (status `harness_only`). `dedicatedserverloop` has no caller in the client code, and the standalone start-up replaces the two CRT functions.

The C++ glue of the Steam SDK (`Set`, `CCallResult`, `CCallbackBase`, `0x10c190`, `0x10c1b0`) is ignored: the `ignored` set of the `client_win` target in `lift.py` turns each call into `0` and lifts none of them. The standalone client has no Steam.

Menu check on 2026-11-08: the main menu draws (title, seven buttons, logo) and the mouse cursor moves. `SR_TRACE=2` prints the first call of each function only: the client reaches `drawmenuitems` and `checkmenuitems` within 2 minutes with no missing function. The image loader (`paeth`, `zhuffman_decode`) takes most of the start time.

C runtime: the client links the runtime statically (`/MT`). `src/standalone/client_crt.c` holds the 51 functions that the game imports from MSVCR100.dll, and `sr_import` takes them from there. MSVCR100.dll is no longer loaded. `instrumentation/tests/crt_diff.c` shows the same bits as MSVCR100 for `sqrt`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `pow`, `atan2`, `ldexp` and `fabs` (500000 inputs each), plus the checks of the server for `sscanf`, `sprintf`, `rand` and `qsort`. A name that is not in the table loads from the DLL and prints a line.

Variable-argument calls (found on 2026-11-08 with the Join and Start buttons): `createmenubutton(int type, ...)` reads its extra arguments by `type`. The decompiler guessed them per call and dropped them at some calls (a pointer to a local, or a constant when the compiler left the stack cleanup to the next call). The lifted C then passed too few arguments and the function read stack garbage. `ApplyCallOverrides.java` fixes this for the functions in `analysis/client_win/call_overrides.csv`: for each call it counts the pushed words (the larger of the `add esp` after the call and the words pushed in the straight code before it: the scan walks back over the block, skips other instructions between the pushes, and stops at a jump, a call or a stack cleanup; a `sub esp, 8` before `fstp [esp]` counts as a double) and writes a call signature override with that many `int` parameters. Run it without `-readOnly`, then `pipeline.sh --export`. The 214 calls of `createmenubutton` now have 2 words for type 1, 3 for types 4, 5, 6, 10, 11 and 13, 5 for type 19 and 8 for type 21 (a double pair and the rest).
Performance and tests (2026-11-09): the lag of the first play tests came from other work on the same PC (gate, bench, builds). A friend ran the zip with no lag. Do not run the gate, the bench or a build while someone plays. `sr_fn` now uses a table with one pointer per code byte (the binary search was 49 percent of the samples). `SR_SAMPLE=<file>` writes the sampler and prints the frame time each 2 s (frames, swap, glFinish, delay). `SR_DENORM=1` counts x87 denormal operands with the exception unmasked.
Gate speed: in reset mode the harness put the snapshot back (about 0.5 s) before each of the 1300 table functions, also for the ones with no recorded call. It now resets only before a function that has calls. The menu recording went from 15 min to 1 min 43 s and the client gate (`--quick`) from 39 min to 9.5 min, with the same counts. `HARNESS_TIMING=1` prints the time of each reset step. Company money bug: the list of companies (press C) printed `$536870912` for each company. The call `sprintf(buffer, "%s: $%d", name, money)` has four pushed words, but the compiler cleans only three after the call and reuses the fourth, so the old count lost the money. The new count fixes it (check on the Windows PC).

Check for lost arguments: `python -I instrumentation/client/check_variadic.py [lifted_client.c]` reads the format string of each `sprintf`, `fprintf`, `sscanf` and `drawtext` call from the exe and compares the number of conversions with the arguments in the lifted C. On 2026-11-09 it finds 0 problems in 89 calls. On the lifted C of 2026-11-05 it finds 7. Run it after each export.

`SR_TRACE` and the crash dump: `menu_dump` in `client_main.c` prints the menu items (name, kind, text pointer and the words at offsets 0x200 and 0x204) when the client crashes.

## Storage of the client (step 7.2)

The clean server code (group C0) reads its data through storage objects (`src/include/sr_storage.h`, from `tools/re/gen_storage.py`). A mixed client build needs the client address of each object.

`tools/re/pair_client_globals.py` finds them. The server and the client share 236 named functions. For each pair, the script compares the lifted C of both. It cuts the text into tokens, aligns the equal parts with `difflib` and pairs the globals at the same place. Each pair is a vote for "this server global is that client global". The script then turns the votes into the client address of the base of each storage object (the field offsets cancel out). The old full lifted C of the server comes from git (`--server-rev`, default `551941a`).

Result: `analysis/client_win/storage.csv` (name, type, count, size, client RVA, votes, total, status). 82 of 138 server objects have a client address (2026-11-09).

| Status | Count | Meaning |
| --- | ---: | --- |
| checked | 18 | The votes and the older candidates (`globals.csv`) agree. Examples: `human`, `player`, `game`, `item`, `city`, `building`, `server`, `option`. |
| ok | 31 | 3 votes or more, and twice the votes of the next address. |
| weak | 20 | Fewer votes. Check by hand before use (small scalars with one vote, `bullet`, `buildingtype`, `connection`). |
| conflict | 3 | The older candidate names another address. The votes win. The difference is the header of the object: `area_sector` +0x40, `masterserver` +4. |
| other | 3 | The votes are weak, so the older candidate wins (`socketid`, `texturelist`, `speex`). |
| dropped | 7 | The object overlaps one with more votes. Look at it with Ghidra when a function needs it (`materials` has 9 of 10 votes but overlaps `area` by 4 bytes). |

56 server objects have no vote. Their functions are not in the shared set, or the client does not have them. The gate finds a wrong address: a function that reads the wrong object gives a difference in group C0.

## Clean server code in the client (group C0)

The client table can point to a clean function of Phase 6 in place of the lifted C. The pieces:

- `analysis/client_win/adopted.txt`: one line for each adopted function (name, source file). `instrumentation/lift/lift.py` reads it. An adopted function has no lifted body. Its table entry has the clean name.
- `analysis/client_win/adopted_pending.txt`: candidates that no recorded call reached. They stay lifted.
- All clean files are compiled into the client (`client_adopted` in `CMakeLists.txt`, with `SR_TEST_MEMORY`). The adapter `instrumentation/client/runtime/storage_adapter.h` (from `tools/re/gen_client_storage.py` and `analysis/client_win/storage.csv`) puts each `sr_data_<name>` at its client address. An object with no client address gets a private dummy (`storage_unplaced.c`). `server_stubs.c` has empty versions of four functions of the server only.
- `tools/re/check_adopted.py --remove` drops the adopted functions that reach a dummy or a server only function.
- `instrumentation/client/adopt_from_gate.py --keep-tested` keeps the functions with a recorded call that was the same and none that differed.
- `tools/re/compare_shared.py` compares the machine code of the functions that have one name in both executables (`analysis/client_win/shared_code.csv`).

Finding (2026-11-09): the server (024d) and the client (0.24b) are not the same build. Of 250 shared names, 6 have the same bytes and 27 have a size so different that the client name is probably wrong (the client `getrandomvector2d` at 0x71e60 is another function). So a clean function is adopted only when the gate shows the same result as the original client. The gate must use every recorded call (`gate.py --all-calls --funcs <names>`): the short call files lost the calls of the vector functions. Result: of 168 candidates, 19 passed (the vector functions, the network functions, `servercycle`, `listfiles` and others), 5 failed (file functions: `building_buildfloorplan`, `loadsaveoptions`, `loadtextfile`, `newspaper_load`, `saveaccountsclient`) and 144 had no recorded call. The clean code calls math through `x87_sqrt` and similar. The client imports the plain `sqrt`, not `_CIsqrt`, so it links `instrumentation/client/runtime/x87_math_client.c` in place of `src/platform/x87_math.c`. Next: synthetic calls or a recording of a hosted game for the 144.

## Image and font loaders (C1)

`src/client/image/` contains PNG, zlib, baseline JPEG, BMP, PSD and HDR decoders, bitmap fonts and byte operations. Each topic has a source file and a public header. The [image documentation](client/README.md) describes the records, supported formats, row filters, channel conversion and font metrics.

All 48 functions are active in `analysis/client_win/adopted_C1.txt`. The pending list has no functions. The fields of nine image and font records are in `analysis/structs/names.csv`. `tools/re/gen_types.py` supplies `sr_image_types.h`. Client data addresses are in `analysis/client_win/group_globals_C1.csv`.

The loaders run on demand. PNG accepts eight-bit samples, five row filters, palette transparency and Adam7. JPEG accepts baseline frames. PSD accepts raw and PackBits planes. HDR supplies float samples. Font atlases retain their glyph widths and float coordinate stores.

## Textures, models and landscape (group C2)

The 46 functions of C2 are clean C in `src/client/textures/` (pages in `docs/client/`). Three more are shared code: `lerpnoise2d`, `serpnoise2d` and `cerpnoise2d` (`src/common/random.c`) and `setupobjecttypeweight` (`src/game/object_types.c`). The list is `analysis/client_win/adopted_C2.txt`. Every function was the same as the client on its tests.

Tests:

- Recorded calls (`gate.py --all-calls --funcs`): `generatetexture`, `generatemipmap`, `generatematerialtexture` (10 calls), `generatehumantexture` (48), `createhumanmodel`, `createhairmodel`, `copymodel`, `createmodel_cityobject*`, `landscape_buildmesh`, `landscape_buildsectormesh` and `landscape_rendersector`.
- Synthetic cases (`instrumentation/client/synth_textures.py <recording> <work dir> [case,...]`): `noise`, `noiseedge`, `hermite`, `models`, `objecttypes` (includes `setupobjecttypeweight`, 24 types), `textures`, `landscape`, `windows` and `materials` (all 62 materials).
- The recordings of `generatematerialwindowtexture` read texture memory that the snapshot does not hold, so the original crashes in the test. The case `windows` writes the second level of four textures into the image and runs the function on it (4 calls, all the same). A pointer written into the snapshot must be a real address: the image starts at `0x20000000` in the test, not at `0x400000`.
- `landscape_buildsectormesh` (14 calls) and `generatemipmap` (6 calls) have skipped calls: the snapshot holds 140 of 178 heap regions.

Facts:

- The client converts a float to an int with `_ftol2_sse` (RVA `0x10c650`): a 32 bit `cvttsd2si`. NaN, infinity and a value outside the range of an int give `0x80000000`. Clean client code calls `cl_ftoi` (`src/client/client.h`). `lift.py` still models the routine as a 64 bit conversion: this changes nothing for the lifted functions that the tests already pass, and a change there touches every lifted function, so it is left for a decision.
- The three grid noise functions keep the shared code, which converts with 64 bits. On 200 calls with normal points they are the same. On the case `noiseedge` (120 calls with NaN, infinity and huge values) 5 or 6 calls of each differ. The texture generators never pass such points.
- The test tools give a stub to each OpenGL pointer slot of `gl_slots.csv` (`instrumentation/client/gen_plat_stubs.py`, `plat_test_patch_globals()`). Functions that call extension functions now run in the test. `__GLeeGetExtensions` (not rewritten) used to skip with "original crashed". It now runs and shows one difference of its return value (a leftover register in the original).
- `zaxis` and `pointtemp` have a client address in `analysis/client_win/storage.csv` (and `globals_checked.csv`), so the server function `setupobjecttypeweight` reads the right memory.
- New client only records are in `analysis/client_win/group_globals_C2.csv`: the model in work, the model and texture lists, the landscape render data, `texture_windowatlas`, `windowrect`, `texture_human`, `bonevert` and `cornervert`.

Result: the gate counts are menu 86, play 311, play2 305 (before C2: 85, 285, 280).

## Sound and voice

`src/client/audio/` supplies device setup, spatial sound, Speex codecs, microphone input, speaker selection, datagram sockets and voice controls. The client resolves OpenAL and Speex calls through `sr_import`. The [audio page](client/audio.md) describes the records and behavior.

Fourteen functions are in `analysis/client_win/adopted_C6.txt`. `voiceoptionsmenu` is in `analysis/client_win/adopted_C6_pending.txt` because no recorded call reaches it. Its window, controls and waveform need an interactive check.

The three client recordings supply 183 matching calls across twelve functions:

- Devices: `setupaudio` 3 and `shutdownaudio` 2.
- Sound: `soundsimulation` 3 and `deletesound` 48. Comparisons include memory changes and the OpenAL call log. OpenAL vector calls contribute to a hash.
- Codecs: `enablespeex` 2, `disablespeex` 2, `preprocessspeex` 48 and `encodespeex` 48. The fixed Speex imports do not test real compression quality or encoded bitstreams.
- Capture: `getaudioinput` 20. Voice is enabled and a capture device is present in all three snapshots. The sample-count import returns zero. These calls test the capture query and its empty result. Sample scaling, clipping, silence detection and microphone hardware need further checks.
- Sockets: `enablesocketserver` 2 and `enablesocketclient` 3. The comparisons test address writes, enable flags and socket calls.
- Icons: `frame_drawvoiceicons` 2. Both snapshots have no active voice channel. Projection, icon appearance and active voice playback need a window check. The processed-buffer import also returns zero, so these recordings do not test voice buffer refills.

`instrumentation/client/synth_audio.py` supplies nineteen cases for each of `getnextvoice` and `addvoice`. Cases cover listeners 0, 1 and 255, free slots 0, 3 and 7, full lists, equal distances, nearer and farther speakers, a blocked comparison key and NaN values. Each call compares its result and memory changes. All 38 calls match. The final distance comparison requires ordered operands and rejects NaN values with a full list.

Timing with every recorded call gives these ratios to the client executable: `soundsimulation` 0.86 across 3 calls, `encodespeex` 0.92 across 48, `preprocessspeex` 1.02 across 48 and `getaudioinput` 0.98 across 20. The same run includes `deletesound`, `scalevector`, `scaleaddvectors` and `vectordistance`. All eight functions stay below 1.05. The combined cycle ratio is 0.920. These measurements use the fixed audio imports and do not measure audio hardware or codec quality.

A separate measurement of `frame_drawvoiceicons` gives 0.97 across two calls with inactive channels.

Whole-client comparisons retain 85, 285 and 280 matching calls for the menu and two play recordings. The server target builds with `SV_BUILD_INSTRUMENTATION=OFF`. Storage checks match 138 initial objects and 276 table and entity digests in both memory modes.

## Setup and main loop, first part (group C7)

Branch `client/c7`. The plan of the group is `build/plans/c7-plan.md` (not versioned). Part A has 13 functions that pass their tests (`analysis/client_win/adopted_C7.txt`): `reset` (the JPEG restart, in `src/client/image/jpeg.c`), `updateplayticket`, `resettimer`, `starttimer`, `debug_addcross`, `debug_addataint`, `debug_addatafloat`, and `debug_writelogfile`. The page is `docs/client/runtime.md`.

Tests: the synthetic cases are in `instrumentation/client/synth_setup.py` (`jpeg`, `timers`, `debug`, `debugoff`, `log`, `ticket`, `mouseon`, `mouseoff`): `reset` 3 calls, `resettimer` 20, `starttimer` 5, `debug_addcross` 4, `debug_addatafloat` 14, `debug_addataint` 116 (one call skipped), `debug_writelogfile` 2 and `updateplayticket` 1, all the same. The test ignores the processor cycle count that `starttimer` stores.

`reactivategame` is written (`src/client/setup/game.c`) but not adopted. It calls functions that use OpenGL, and the offline test skips them. It is in `analysis/client_win/adopted_C7_pending.txt` until a test with the game window shows it works. The Steam functions (`steam_*`) have no clean code. They are glue for the Steam SDK, and the client keeps them as they are.

`reset` was in the list of C7 only because of its generic name. It belongs to the JPEG decoder, and the image code calls it directly now.

Part B also has `clientsend`, `humansimulationclient`, `itemsimulationclient` and `client_simulation` (pages `docs/client/network.md`, `src/client/setup/send.c` and `interpolation.c`). `client_simulation`, `humansimulationclient` and `itemsimulationclient` pass 4 synthetic states each (`world` to `world4`: humans on foot and in vehicles, objects, items with every holder). `clientsend` passes 4 synthetic calls (`send1` to `sendnone`): three players with different action rings and voice rings, and one with no player in view. The synthetic test caught a wrong address of `last_event` (the 16 bit field in the packet): check the addresses of new globals as image address minus 0x400000.

`checksimulation` is adopted (`src/client/setup/simulation.c`, page `docs/client/simulation.md`). Its 10 recorded calls are all skipped: the original crashes in functions that `checksimulation` calls (`lineintersectspecblock` and one at 0x4a180), because the snapshot does not hold the heap that they read. The synthetic cases give 22 calls, all the same: a host with state 3, with keys and with the debug flag (`tickhost`, `tickkeys`, `tickkeys2`, `tickdebug`, `tickshift`), and a client with state 3 (`tickclient`, `viewfoot`, `viewcar`, `viewheli`: a player in view that is on foot, in a car and in a helicopter). What no test reaches is the part that runs only in the states 0, 1 and 2: the world functions of the tick, the traffic lights and the fade of death. The states 1 and 2 crash the original in the collision code. That part follows the old code line by line. A test with the game window covers it.

The gate counts do not change: menu 86, play 311, play2 305.

### Functions whose original crashed in the test (2026-11-11)

Every function that opens a file used to be skipped with "original crashed". Cause: `checkbigendian` stores the addresses of `fread` and `fwrite` in two globals (`0x908b064` and `0x13bee680`). The snapshot holds the addresses of the recorded process, and a call through them ran in memory that does not exist in the test. `plat_test_patch_globals()` patched two server addresses, not these two. It now patches the right ones for the client. After the change `lf_34010`, `lf_345d0`, `area_createsblocks`, `area_createinteriorblocks`, `building_load`, `leveleditor_load`, `setupgame` and `setupclient` run and give the same result in the lifted and the original code. `resetgame` and `createlevel` still skip: they call `generatematerialwindowtexture`, which reads texture memory that the snapshot does not hold. `HARNESS_CRASH_STACK=1` makes the harness print the registers, the page of the crash address and the return addresses of the stack.

## Open work

About 400 functions have no name (2510 of 2907 are named). 39 of them are the `__GLeeLink_WGL_*` functions at `0x2df70` and up: the Linux client has GLX links in their place, so no reference gives their names. About 70 of the rest have 512 bytes or more. They sit in `0x0` to `0x10000` and from `0x30000` up. `tools/re/name_glee_order.py` named 171 GLee link functions by their order in the Linux client (`analysis/client_win/glee_order_names.csv`).

Three passes name functions that the first matcher pass left open. All are in `name_all.sh`:

- A strict pass against the 024d, which has the same compiler. Many of its names have low confidence (0.333 with 2 votes). Check them before a rewrite.
- `tools/re/match_strings.py`, which compares only the strings of a function.
- `tools/re/match_callees.py`, which compares the sets of named functions that a function calls. It also needs a callee overlap of 4 names, a gap of 0.2 to the next name, and some shared constants. It named 21 functions, among them `objectsimulation`, `objectwheelsimulation2` and `area_buildsectormodel`.

The 0802 server names come from this client, so they cannot name the client without circular evidence.

See `docs/plano.md`, Phase 7, for the steps.

## Ghidra MCP for the client

The MCP server opens the 024d by default. For the client, start it with the program path in the environment:

```bash
GHIDRA_MCP_BINARY=original/client/subrosa.exe powershell -File instrumentation/ghidra/mcp_server.ps1 restart
```

The copy of the project has no function names of the client. Ghidra shows `FUN_` names. Look up each address in `analysis/client_win/functions.csv` (RVA = address minus `0x400000`).

## Rendering, first part (group C3)

The page is `docs/client/render.md`. The code is in `src/client/render/`.

Adopted (`analysis/client_win/adopted_C3.txt`): the 2D shapes (`drawbitmap`, `drawbitmaptexcoord`, `drawlinebox`, `drawmousecursor`, `drawcircle`, `drawcirclefill`), `drawtext`, `drawhelp`, the round indicators (`drawplayerindicator`, `drawcircleindicator`), the OpenGL states (`enabletwosidedstencil`, `disabletwosidedstencil`, `glstatenormal`, `glstateshadowtwoside`), `compileprogramobject`, the drawing of models, landscape, sky, shadow and map (`rendermodel`, `rendermodelwire`, `rendermodelnormals`, `rendersphere`, `rendertrackmodel`, `renderlandscape`, `renderframeatmosphere`, `rendermodelshadow`, `rendermap`), the view cone (`boxinsidefrustum`, `pointinsidefrustum`) and the debug boxes (`debug_renderbox`, `debug_renderboundingbox`).

Not adopted (`adopted_C3_pending.txt`): `setupprogramobjects`. It calls OpenGL extensions through the slots, so the offline test skips it. It needs a test with the game window.

Tests:

- Recorded calls (`gate.py --all-calls --funcs`): `drawbitmap`, `drawmousecursor`, `drawcircle`, `drawplayerindicator`, `drawcircleindicator`, `drawhelp`, `compileprogramobject`, `rendermodel`, `rendertrackmodel`, `renderlandscape`, `renderframeatmosphere`, `rendermodelshadow`, `rendermap` and two of the OpenGL states. All the same. The test compares the hash of every OpenGL call, so a wrong vertex shows as a difference.
- `drawtext` has a variable argument list, so the test cannot call it. Its callers `drawdisplay`, `drawhud`, `drawhelp`, `drawplayerinfo` and `frame_drawmessages` call the clean `drawtext`, and their OpenGL hash and memory are the same.
- Synthetic cases (`instrumentation/client/synth_textures.py <recording> <work dir> frustum,frustum2,draw2d`): `frustum` and `frustum2` call `boxinsidefrustum` and `pointinsidefrustum` 160 times each with two cameras, all the same. `draw2d` calls the 2D shapes, the states, the debug boxes, `rendersphere`, `rendermodelwire` and `rendermodelnormals` 60 times each, all the same (5 calls of `rendersphere` skipped because the original reads memory that the snapshot does not hold).
- `rendermodelshadow` has 17 and 8 calls that the test skips for the same reason.
