# Final plan of the client

Goal: a client that people can read, optimize and change. Several people will work on it.
The normal build has no image of the original executable, no address in a call, and no tool
of `instrumentation/`. The code is shared with the server where the algorithm is the same.

The end state is a source tree that reads like the original source of the game. A developer
opens a file, reads it, and changes it. No custom layer stands between the code and the compiler:
no hook macro, no call by address, no storage adapter, no generated offset.

## State on 2026-11-25

- All game code has clean C: about 52000 lines in `src/client`, `src/game`, `src/common` and `src/net`.
  The only lifted C that is left is GLee (third party, built from `third_party/glee`).
- The native target `client_native` compiles all 200 source files with 0 errors. Each datum is a real C
  object (`src/client/storage.c`, `src/include/sr_client_storage.h`). Each call goes by name.
- Not done: the native executable (link and start), the test only scaffolding in the clean code,
  direct calls to the system libraries, names for 20 fields, a policy for changes.

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
3. SDL2, OpenAL, Speex: put the source in `third_party/` and build it into the executable (the
   project is GPL, the three licenses work with it: zlib, LGPL, BSD). Call the real API.
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

## Phase E. Rules for changes

The decision that opens the work for other people: what happens to the comparison with the original.

- Until now each function has to give the same result as the original. This is the right rule
  while the code is a copy.
- After phase A, freeze a reference: the recordings and the synthetic tests give the results of
  the last compared version. A change that does not alter a result passes the same tests.
  A change that alters a result (a new feature, a faster algorithm, a fix) says so in the
  commit, and the owner updates the reference for that function.
- Protocol changes need the server and the client to change together.
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
   or an accepted result, copies of data, repeated calls.
3. Check the denormal cases (small integers in float locals are slow on x87).
4. Later, after the code is stable: a 64 bit build. The pointer size asserts in the types show
   each place that has to change.

Model: Opus for the float work. Sonnet for the rest.

## Order and lanes

1. Phase A first (it proves the native code runs). It blocks B, C and F.
2. In parallel with A: D (names and docs). They do not touch the link.
3. After A: B and C in two lanes. B changes calls and data names. C changes the library calls.
   They touch different files, so two worktrees can run together.
4. E when A passes. F after B and C.

## Decisions (items 1 and 2 confirmed by the owner on 2026-11-25)

1. Phase E: after the freeze, a change that alters a result is allowed with a note in the commit.
2. Phase B step 4: the harness, lifter and recorder stay in the repository, outside the normal build.
3. Phase F step 4: a 64 bit build comes after phase F, not before.
