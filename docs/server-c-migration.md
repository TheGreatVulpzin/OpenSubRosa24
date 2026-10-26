# Independent C server storage

## Reference

The starting branch is `main`, at `eb68519`. The working tree is clean.
The MSVC x86 Release build passes before this change.
The type, network and verifier executables pass. The 12 Python tests pass.
Full gate and timing logs are in `build/c-migration/`.
That directory stays outside version control.

The file `src/lifted/lifted_024d.c` has no function bodies.
The remaining server dependencies are address dispatch in startup and accounts,
data access in the shared runtime, and the image allocation in startup.
The x87 thunks contain handwritten assembly and need a normal runtime home.

## Steps and completion criteria

1. Replace address dispatch with declared C calls. Check startup, accounts and the tick loop.
   Keep the hook build able to call the game when differential tests need that path.
2. Define separate typed storage for each game pool, record, table and counter.
   Generate initial values from the checked data metadata. Keep capacities and pointer aliases.
   Put the memory adapter for hooks, recordings and the harness in `instrumentation/`.
3. Remove image allocation, address tables and lifted sources from the normal target.
   Keep the recording target and differential tools operational.
4. Run the build, full gate, focused tests, synthetic cases and timing checks.
   Add checks for native storage, initialization and the normal target dependency closure.
   Record results without changing gate references.

Completion requires both direct calls and independent data storage.
A renamed address macro or a hidden image block does not meet this requirement.
The normal server must build without an instrumentation include path or lifted source.
No floating point expression or rounding point needs to change.

## Rollback

Each structural step has its own commit. Revert the commits in reverse order.
The recording target retains the image adapter for differential verification.
No saved game or account format changes.

## Storage and aliases

The normal target defines 138 separate C objects, with 499,799,832 declared bytes.
The linker places those objects. No runtime function reserves or reconstructs a game image.
Pool records retain the sizes from `sr_types.h`. Capacity assertions cover every storage object.

The storage containers preserve these aliases:

- Account keys and counts precede 65,536 account records by eight bytes.
- Account file operations share `Parser.file`.
- The sector header precedes 8,192 `BlockSector` records by 64 bytes.
- Surface points and UVs share their control-grid record.
- Packet triangle scratch uses the camera storage.
- The 41,368-entry kick list shares a typed union with packet records and item types.
- The landscape reset counter remains the last word of `Newspaper`.
- The completed item scan still reads the float at offset 0x48 of event 11,194 as its team angle.

`tools/re/gen_storage.py` owns the definitions, declarations, adapter and test region list.
It rejects overlapping storage regions. `--check` detects stale outputs.
Initialized scalar values, vectors, tables and strings become C initializers.
The default comparison checks storage bytes exactly and checks 15 initialized string pointers by content.

`SR_TEST_MEMORY` selects the adapter only for verification targets.
The adapter places the same typed containers at their test-image addresses.
The normal target has neither that definition nor an instrumentation include path.
`tools/build_server.bat` builds it with `SV_BUILD_INSTRUMENTATION=OFF`.

The hook loop still enters installed hooks through `instrumentation/runtime/loop_calls.h`.
System imports, file callbacks and other valid function pointers remain indirect.
The recording target keeps its image, relocations, function metadata and snapshot address format.
Its separate object library instruments game C with `/Gh`.
The normal executable has no entry instrumentation.

## Verification results

Both clean MSVC x86 Release builds pass: the normal target alone and all verification targets.
The CMake dependency test checks the normal source list, include paths, compiler options and map symbols.
It also checks game sources for address dispatch and image access.
The normal target has no image allocation, function table, lifted implementation or verification adapter.
The server still needs Windows x86 and game data files.

The full gate passes on 13 recordings, with 2,087 matching calls and no reduced counts.
Gate references do not change.
The focused all-calls checks request these functions:

```text
genrand_int32, loadaccountsserver, createaccount, loadaccountsclient,
saveaccountsclient, saveaccountsserver, setupgame, resetgame, servercycle,
serverrecv, serversend, setupobjecttypes_cars, setupobjecttypes_helicopters,
setupobjectypes_train, createobject, createvehicle, ai_playercar,
ai_car_nextwaypoint
```

Sixteen functions have matching recorded calls.
The recordings do not exercise `loadaccountsclient` or `saveaccountsclient`.
A separate file test saves and loads all 65,536 client records in both memory modes.
It checks the file version, count, size and every loaded value.

The final `step.sh` summary is:

```text
all-calls gate: ok (13 recordings)
synth types: ok (28 cases)
synth vehicles: ok (5 cases)
security: 38 cases, 0 failed
invalid input: ok
```

The storage test compares all 138 initial objects against the image adapter.
It compares 276 state digests after table setup and entity creation.
Both memory modes also pass nine boundary cases.
These cover short reads, disconnected bit reads, packet workspace aliases, kick-list capacity and newspaper capacity.
The tests use separate temporary folders.

| Synthetic suite | Result |
| --- | --- |
| Types and AI | 28 passed |
| Vehicles | 5 passed |
| Humans | 17 passed |
| Bonds | 1 passed, with 10 bonds |
| Items | 76 passed |
| Network | 278 valid cases passed |
| Area | 60 passed |
| Logic | 45 matched, 1 existing expected difference |
| Level | 20 passed |
| Buildings | 180 passed |
| City objects | 12 matched, 1 pre-existing difference |
| System | 51 matched, 2 existing known differences, 1 pre-existing difference |
| Invalid input | 38 passed |

Two raw differential cases still fail: `cs_pread_short` and `newspaper_overflow`.
The saved pre-migration harness reproduces both failures.
They compare existing C input guards with unsafe behavior in the reference executable.
No exception list or reference changes hide these failures.
The native boundary tests check that both memory modes preserve the existing C guards.

Four network sentinel cases initially used an already disconnected receive buffer.
The saved harness reproduces those differences too.
The valid fixtures now start with a loaded receive buffer.
Separate security cases retain the disconnected input and check its guard behavior.
A Python test checks these fixture preconditions.

The type, network-guard and verifier executables pass.
All 14 Python tests pass.
The recording server writes a snapshot and calls after a clean start.
Replay checks 30 functions and 125 calls, with no differences, crashes or skipped calls.
The CRT comparison retains its existing `sprintf` differences.
Its random, sort, character and floor checks report no differences.

## Timing and limits

The benchmark checks 275 functions before and after the migration, with identical call counts.
The aggregate C/reference ratio changes from 1.014 to 1.010.
No function with at least 20 calls gains a new slowdown above five percent.
The per-function counts and ratios are in [server-c-migration-bench.csv](server-c-migration-bench.csv).
These differential timings use the verification adapter.

The normal server also starts and stops with private test data.
The idle rate changes from 62.5 to 62.3 ticks per second, a ratio of 0.997.
This idle check does not measure multiplayer load.

No record sizes, account formats, field meanings or floating point expressions change.
`names.csv` needs no new field names.
The storage header declares the new `sr_data_*` objects and preserves the existing runtime names.
There are no uncertain rounding points from this change.
Review by another model remains necessary, especially for pointer aliases and storage capacities.

## Reproduction and build checks

```text
tools\build_hook.bat
tools\build_server.bat
bash instrumentation/lift/pipeline.sh --no-gate
python -I instrumentation/lift/gate.py
python -I instrumentation/lift/verify_storage.py
python -I tools/re/gen_storage.py --check
python -I -m unittest discover -s instrumentation/lift -p "test_*.py"
```

Run the focused functions above with `instrumentation/lift/step.sh`.
Use recordings with the required AI and vehicle state for those synthetic suites.
Run `instrumentation/lift/bench.py` with the functions and their C callees.
Run `instrumentation/tests/test_native_server.py` with a saved server and private test data for idle timing.

The pipeline regenerates the lifted sources, tables and hooks.
Their generated contents stay unchanged because no function bodies remain to migrate there.
Its inventory still reports 316 C replacements and five handwritten routines.
Two unused decompiler candidates still fail its compile filter.
Those candidates do not enter the normal server or replace any tested C function.
The storage generator owns its four generated outputs.
The type generator also owns the new capacity header.
No generated output needs a manual edit.

An incremental MSVC build initially kept stale objects after a storage-header change.
A clean build removed those objects and passed.
CMake now lists the storage headers as explicit object dependencies.
`instrumentation/lift/rebuild_clean.bat` builds both configurations from fresh CMake state.

Private logs, snapshots and saved binaries remain under `build/c-migration/`.
No recordings, configuration files, game binaries or generated build files enter these commits.
No release, package update or push is part of this migration.
