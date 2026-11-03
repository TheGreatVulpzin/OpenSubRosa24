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

`cl_wsock32` also holds the lifted client code, the hook tables of the lifter, the verifier and the recorder of the server (`verify.c`, `record.c`, `record_imports.c`). `instrumentation/client/cl_hooks.c` installs them. They need `build/lift/client_win/lifted_client.c`, so run `lift.py --target client_win` first.

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

Order after a change of names or types: `ApplyImports.java`, `LiftExport.java`, `lift.py --target client_win --compile`, `gen_count_table.py`, `gen_plat_stubs.py`, `toolsuild_hook.bat`.

Result (2026-11-03, menu recording): 718 lifted, 471 harness only, 34 side effect (the loops and their callers), 12 compile errors. The harness tests 84 functions: 131 calls same, 0 different, 162 skipped.

Two name errors of the matcher showed in the CRT area (`0x10c000` and up): `0x10c650` is `_ftol2` (it takes ST0), and `0x10c6fb` is `__security_check_cookie`. The matcher names in that area are not reliable. `FixRegisterPreservation.java` and `FixCookieCheck.java` take the address as a script argument.

## Open work

About 570 functions of the game code have no name (about 80 of them have 512 bytes or more). They sit in `0x0` to `0x10000` and from `0x30000` up.

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
