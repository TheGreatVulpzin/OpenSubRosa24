# Final plan of the client

Goal: a client that people can read, optimize and change. Several people will work on it.
The normal build has no image of the original executable, no address in a call, and no tool
of `instrumentation/`. The code is shared with the server where the algorithm is the same.

The end state is a source tree that reads like the original source of the game. A developer
opens a file, reads it, and changes it. No custom layer stands between the code and the compiler:
no hook macro, no call by address, no storage adapter, no generated offset.

## State on 2026-11-27

- The native client with guard bytes (`bash tools/native_test.sh --guard --hidden menus play crash buildings level model`): all six scenarios pass and no guard fires.
- Frame time at the main menu, measured as the CPU time of the process in 30 seconds (same folder and options, 5 runs of 30 seconds each, script not versioned). The original client used 18.5 to 30.6 percent of one core (mean 24.6). The native client used 16.0 to 29.0 percent (mean 20.6). The runs vary a lot, and some ran while a build was running. The native client is not slower at the menu. A scene with play (city, fight) is not compared yet.
- The type headers are plain source. `tools/re/gen_types.py` is gone. `tools/re/record_tables.py` holds the record sizes and the array capacities, `tools/re/check_types.py` compares the headers with each other, and `tools/re/gen_address_headers.py` writes only the two address headers of the verification targets. A new field goes into `src/include/sr_types.h` and `analysis/structs/names.csv`. `GameClient` and `OptionClient` are written by hand now.
- `instrumentation/recording/force.py` and `instrumentation/hook/force.c` make the scenario forcer for recordings (`docs/rerecord.md`).
- `instrumentation/ghidra/types_json.py` reads the current header again (177 records, each size equal to its `static_assert`). It handles pointers, several names with their own dimensions in one line, two dimensions and the `raw` form of a union. `ApplyTypes.java` has not run on the new JSON: run it once before the next Ghidra work.

## State on 2026-11-26

Phases B to F are done in the code of the client:

- B: every call goes by name (no `CL_FN_NAMED`). The aliases of the storage are fields of the records (one name, `sr_data_view`, stays a macro for a field). `src/include/sr_variant.h` is the one place where the client and the server differ. The image executable targets still exist in `instrumentation/`, but they do not build all: `cl_wsock32` and `harness_client` need a new hook table.
- C: SDL2, OpenAL, Speex, OpenGL, the C runtime, winsock and the shell are normal calls. `sr_import` is gone from `src/client`, `src/game`, `src/common` and `src/net`. `src/platform` and `src/server` still use it for the operating system calls.
- D: no `unk_` name is left. `docs/README.md` maps each system to its folder and page.
- E: `CONTRIBUTING.md` and `docs/changes.md` say what a change may do.
- F: a sampler and `tools/re/profile_samples.py` show where the client spends time (`docs/client/performance.md`). The client has its own `rand`. The frame time was not compared with the first executable.

Later on 2026-11-26:

- The native client has no `sr_import`. `src/platform/os_calls_native.inc` and the native branch of `x87_math_client.c` call the C runtime, winsock and KERNEL32 directly. The executable imports only KERNEL32, OPENGL32, SHELL32, WS2_32, SDL2, OpenAL32 and the Speex DLL files.
- B4: the root `CMakeLists.txt` holds the native client and the server. All verification targets (hook libraries, harness, recorder, image executables, storage checks) are in `instrumentation/targets.cmake`, which the root includes only when `SV_BUILD_INSTRUMENTATION` is ON. With it OFF, the server and the native client build.
- The owner accepts the server gate coverage of the recordings that came back (`20261013-query`: 188 functions, 2230 calls same). The gate lists the functions of the lost recordings as "DROP". The baseline is not updated.

- The image targets `cl_wsock32` (the recording proxy) and `harness_client` build and work again. `gen_system_thunks.py` gives them the system library calls (OpenGL, SDL2, OpenAL, Speex, winsock). The lifter leaves a rewritten function without a hook when its first bytes hold a relative call (`setupobjecttypes` crashed the recorded client). The harness sees a call only when it goes through `src/include/sr_os.h`: `os_fread`, `os_fseek`, `os_realloc` and the other file and memory calls. The image loader called `realloc` directly, and its buffer was freed by another allocator.
- Check on a new recording of the original client at its menu with nobody playing (`build/recordings/client-menu-*`, 20 s snapshot), through the client gate: 160 functions tested, 367 calls the same as the original, 0 crashed. The two differences are the harness copy of the third party GLee and `steam_init` (known: no Steam in the client). 57 calls are skipped by the harness. Only the menu is covered. The test environment (`plat_test.c`) has a `fprintf` stub (the snapshot holds file handles of the recorded process), and `gen_system_thunks.py` also defines the shell calls and `_chdir`, `_getcwd` and `_mkdir` for the image targets. The client gate (`client_win.json`) lists the new recording and still lists the three lost ones.

Not done: `sr_import` in `src/server` and in the `src/platform` files that only the server and the image targets use (`x87_math.c` needs the `_CI` math forms, and the test environment replaces the other calls). The client gate has no baseline: record a play session with the launcher and run `gate.py --target client_win --update` (ask first). The frame time against the first executable.

## State on 2026-11-25

- All game code has clean C: about 52000 lines in `src/client`, `src/game`, `src/common` and `src/net`.
  The only lifted C that is left is GLee (third party, built from `third_party/glee`).
- The native target `client_native` compiles all 200 source files with 0 errors. Each datum is a real C
  object (`src/client/storage.c`, `src/include/sr_client_storage.h`). Each call goes by name.
- The native executable `opensubrosa24_client_native.exe` links with 0 errors (1.6 MB, no image). It shows the
  main menu, and the Demo shows the city with the HUD, the gun and the items (checked by hand on 2026-11-25).
  `SR_SAMPLE=1` and `tools/re/map_address.py` show where the main thread is. A crash prints its address and
  the return addresses on the stack, for the same tool.
- One bug of the first run: each datum is its own object, and the original executable writes past the end of an
  array into idle data. The list of the interior shadow volumes did this and broke the light, the track
  buffer and the viewport. `analysis/client_win/native_slack.csv` gives an array more room in the native build.
  Another array can do the same: a wrong value that changes between frames points to it.
- Not done: the smoke test of the native executable on all scenarios, the test only scaffolding in the clean
  code, direct calls to the system libraries, names for 20 fields.

## Numbers that size the work

| Item | Count |
| --- | ---: |
| `sr_import` lookups of a system function by name | 108 (CRT 56, winsock 24, SDL2 10, kernel32 6, shell32 2) |
| `CL_FN_NAMED` calls that still carry an address | 179 in 48 files |
| Data aliases with a fixed offset | 87 (20 have no field name yet) |
| `unk_` names in the clean code | 9 |
| System docs in `docs/game` and `docs/client` | 48 files |

## Phase A. The native executable runs

Done when: the native executable opens the menus, enters the Demo, shoots with sound and
particles, opens the editors, and joins a local server. `smoke.py` gives the same result for
the native executable and the image executable.

1. Import layer for the native build: one small file with the system functions that the game
   needs. Replace the `sr_import` lookup of `MSVCR100.DLL` by the static CRT (real headers).
2. Split `src/standalone/client_main.c`: the crash filter, the sampler and `--run` stay. The start
   of the image goes to its own file (`instrumentation/standalone/`). The native start calls
   `cl_storage_init()` and then `client_main()`.
3. Link: `client_native` objects, `storage.c`, `third_party/glee/GLee.c`, `opengl32`, the system
   libraries. Fix each missing symbol.
4. The smoke probes read the game state from data, not from calls (`sr_data_game.state`).
5. `smoke.py --compare` of the native and the image executable on all scenarios.
6. Manual test with `jogar.bat` (the list of 2026-11-24).

Model: Sonnet for 1 to 5. Opus only if a crash in the first start has no clear cause.

## Phase B. Remove the test scaffolding

Done when: no file in `src/` has an address of the original executable, and the normal client
target has no include path to `instrumentation/`.

1. Replace each `CL_FN_NAMED(name, address, type)` by a direct call `name(args)`. Add the header
   that declares `name`. The type cast goes away.
2. Replace the 87 aliases by named fields of the big struct (names in `analysis/structs/names.csv`
   and `tools/re/gen_types.py`). The generator stops writing offsets.
3. Remove every hook macro of `client_build.h`. Each one becomes normal code, one of two forms:
   a function that the shared code calls by name and that the server and the client each implement
   (the linker picks one, for example `unpack_state_objects`), or one shared function when the
   difference is only data (the two record layouts become one). No macro that expands to a
   different expression in the client.
4. Keep `instrumentation/` as an optional verification tree (harness, lifter, recorder) outside the
   normal build, as the server does. The image executable target moves there.

Model: Sonnet. Steps 1 and 2 are mechanical and can use a script plus the compiler as the check.

## Phase C. Use the system libraries as a normal program

Done when: `grep sr_import src/` finds nothing in `src/client`, `src/game`, `src/common`, `src/net`.

1. CRT (56 lookups): call `fopen`, `fread`, `qsort` and the others with the real headers.
2. winsock (24): `winsock2.h`, link `ws2_32`. The game used `WSOCK32.dll` by ordinal: use the names.
3. SDL2 and OpenAL: keep the DLL files that the game ships. Add their headers to `third_party/` and link the
   import library, so the code calls the real API by name. Speex: put the source in `third_party/` and build it
   into the executable (BSD). OpenAL Soft is LGPL, and a DLL is the simple way to follow its license.
4. kernel32 and shell32 (8): `windows.h`.

Model: Sonnet. Each library is one commit and one smoke test.

## Phase D. Readable code

Done when: a new developer can find a system, read it and change it without the notes of the project.

1. Name the 20 fields without a name. Remove the 9 `unk_` names.
2. Check each source file: the header comment says what the file does. Long functions split into
   named helpers when the split does not change a result.
3. One document for each system in `docs/` is current (the 48 files exist). Add a map file:
   `docs/README.md` with a table "system, folder, document".
4. Comment style of `CLAUDE.md` over the whole tree: no process words.

Model: Haiku for the grep checks, Sonnet for the names and the splits.

## Phase E. Scope and rules for changes

OSR24 (this repository) gives the source of the game in the hands of the reader, with few changes.
It keeps the vanilla behavior: the protocol, the simulation and the results of every function.
New features do not go here. They go to Overrosa (the client with extensions) and to SubVulpes
(the server). Both use this repository as their base.

- SubVulpes talks to the vanilla game wherever it can. The vanilla path of the protocol stays
  the code of this repository. A feature that a vanilla client cannot read is sent only to a
  client that announced support for it (an extension flag at the start of the connection).
- A change here is allowed for a security fix, a crash fix, a build fix, a gain in speed that
  keeps every result bit for bit, and a gain in readability. A change that alters a result is
  rare. Its commit says what changed and why, and `docs/changes.md` lists it.
- Until phase A, each function has to give the same result as the original. After phase A, freeze
  a reference: the recordings and the synthetic tests give the results of the last compared
  version. A change that passes the same tests did not alter a result.
- The build has one command, the tests have one command, and a short `CONTRIBUTING.md` says both.

## Phase F. Speed

Done when: the frame time at the same scenes is not worse than the original, and each
hot function has a measured result.

0. Benchmark of the rewrite (`python -I instrumentation/lift/bench.py`): the time of each clean
   function against the original, for every function that runs each frame. Run it once after
   phase A and again after phase B. A ratio above 1.05 in all runs is a slower function: fix it
   before the next phase.
1. Profile with `profile=1` and `SV_SAMPLE` on the native executable: frame time of the menus,
   the city, a fight.
2. Remove what only existed to match the original: x87 helpers where SSE gives the same
   result bit for bit, copies of data, repeated calls. No change that alters a result.
3. Check the denormal cases (small integers in float locals are slow on x87).
4. A 64 bit build is not planned for this repository. It changes the layout of the records, so the
   owner decides it later. The pointer size asserts in the types show each place that would change.

Model: Opus for the float work. Sonnet for the rest.

## Order and lanes

1. Phase A first (it proves the native code runs). It blocks B, C and F.
2. In parallel with A: D (names and docs). They do not touch the link.
3. After A: B and C in two lanes. B changes calls and data names. C changes the library calls.
   They touch different files, so two worktrees can run together.
4. E when A passes. F after B and C.

## Decisions

1. Confirmed 2026-11-25: the harness, lifter and recorder stay in the repository, outside the normal build.
2. Confirmed 2026-11-25: this repository keeps the vanilla behavior. Features go to Overrosa and SubVulpes.
   SubVulpes talks to the vanilla game where it can, and uses extensions only with a client that announces them.
3. Confirmed 2026-11-25: libraries. SDL2 and OpenAL as DLL with headers and import libraries, Speex and GLee as source in
   `third_party/`. All calls go direct, without `sr_import`.
