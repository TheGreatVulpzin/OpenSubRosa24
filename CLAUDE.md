# CLAUDE.md

RE/decompilation project for Sub Rosa (2015).

**Language:** everything in the repository is in English (code, comments, docs, commit messages). Chat with the user is in Portuguese (BR). Old commits stay in Portuguese.

**Comments:** write code comments in ASD-STE100 style (short sentences, active voice, simple words). No emojis. No dashes as punctuation (use a period or a colon). Keep each comment short: say why, not what the code already shows.

## Rules

- Never version anything in `original/` except `README.md` and `MANIFEST.sha256`. Binaries and assets are proprietary.
- C tables generated from the 024d `.rdata` and initial `.data` (strings, constants, default values) may be versioned in `src/` (Phase 5.1). The user checked them.
- `original/` is read-only. Tools read from it and write to `analysis/`, `build/` or the scratchpad.
- Run Python with `-I` when reading files from `original/` or `third_party/`.
- The servers' `config.txt` has passwords. Do not copy its content into docs or commits.
- Recordings (`build/recordings/`) hold player names, IP addresses and `config.txt`. Never commit them or copy them into docs.
- **Ghidra is not part of the daily loop of the server.** This rule is for the server (Phase 6). Phase 7 (the client) uses the full method again: Ghidra, the lifter and the names. Phase 6 rewrites each function by hand into clean C. The lifted C is the fallback and the reference for the rewrite. A new name goes into `analysis/structs/names.csv` (and `tools/re/gen_types.py` for a custom type), then `gen_types.py`, in seconds. Until the lifted C has its own frozen header (`docs/plano.md`), a name that leaves `sr_types.h` breaks the lifted C: then put all the names of a lot into one ApplyTypes run. Use Ghidra only to find new facts (an unknown function, a call target, a data layout).
- **Never fix the lifted C with text rules.** Never add a regex rule to `tools/lift/lift.py`. When the lifted C of a function is wrong, rewrite that function by hand, or find what Ghidra does not know and fix it with a Ghidra script. The only text exception is `LIFT_PATCHES` in `lift.py`: one explicit fix for one decompiler bug in one function.
- A hand-written function must give 0 harness differences and must not be slower than the original. Measure with the profiler (`subvulpes.ini profile=1`). `src/server/main_loop.c` is an example of code that works but is not well optimized.
- **Gate cost.** The gate runs the short call files (`sv_args.min.txt`, a few calls for each code path, made by `tools/lift/min_calls.py`). While you rewrite a function, run `tools\build_hook.bat` and `gate.py --all-calls --funcs <name>` (2 to 4 min), plus the synthetic tests (`tools/lift/synth_bonds.py`, `synth_humans.py`) for the paths that no recording reaches. Use `pipeline.sh` (LiftExport, lift, build, `gate.py --changed --quick`) only after a change in Ghidra. Before a push, run the full gate (`pipeline.sh --full` or `gate.py`, about 3 min). Run `gate.py --all-calls` (every recorded call, 30 to 40 min) only now and then, for example once a week. After a new recording, run `min_calls.py` and `gate.py --update`.
- Every lifter change must keep the harness results: no "calls same" count may drop on any recording (see `docs/lift.md`).
- Knowledge that must stay goes into the repository (`analysis/*.csv`, `src/include/sr_types.h`, scripts), never only into the Ghidra project. The Ghidra project is not versioned.

## Key binaries

- `original/client/subrosa.x64` — Linux ELF64 **with symbols**. Source of truth for function names.
- `original/client/subrosa.app/Contents/MacOS/subrosa` — Mach-O i386 with symbols. Reference for the 32-bit layout.
- `original/blackhouser/subrosadedicated024d.exe` — PE32 without symbols. Target of the server manager (SHA-256 `11e00678…`).
- 024d addresses are always **RVAs** (relative to the module base), as in the server manager.

## Cloud session (Linux)

The cloud does the heavy Ghidra work. Building with MSVC and testing with the game stay on the Windows PC.

1. Run `bash tools/cloud/setup.sh`. It installs Java 21 and Ghidra 12.1.4 and downloads the official binaries from gart.sh. It takes 5 to 10 minutes.
2. Run `bash tools/ghidra/analyze_all.sh` to create the Ghidra project. It takes 10 to 30 minutes.
3. Commit only the versionable results (`analysis/`, `docs/`, `src/`, `tools/`). Never commit `original/`, `build/` or `ghidra-project/`.

If the download fails, the session needs network access to `crypticsea.gart.sh` and `github.com`.

## Tools

- `tools/re/strings.py <file> [min]` — ASCII strings.
- `tools/re/elf_symbols.py <elf> [--sections]` — ELF functions.
- `tools/re/survey_bins.py <folder> [--manifest …] [--tsv]` — format, symbols and SHA-256 of every executable in a folder.
- `tools/re/manifest.py write|check` — hashes of `original/`.
- `tools/ghidra/analyze_all.sh [name]` — imports and analyzes binaries. Paths are in `tools/ghidra/env.sh`.
- `tools/ghidra/export_all.sh` — re-exports features without re-analyzing.
- `tools/ghidra/decompile.sh <program> <output.c> [rva ...]` — pseudo-C. Program in a project folder: `mac025/subrosa`.
- `tools/re/name_all.sh` — generates the function-name CSVs.
- `tools/re/map_globals.py` — maps global variables by votes.
- `tools/re/map_regions.py` — maps unnamed globals to symbols by the functions that use them (`--test` checks it on the known globals). Its output is a list of candidates: check each one before it goes into a CSV.
- `tools/re/struct_fields.py` — collects struct fields from the pseudo-C.
- `tools/formats/subrosa_formats.py` — readers for `.srk`, `.sbl`, `.sbb`, `.sbc`, `.csm`.

### Ghidra database (types and names)

- `tools/re/gen_types.py` — generates `src/include/sr_types.h` from `analysis/structs/names.csv`. Add a known field there, never in the header.
- `tools/ghidra/types_json.py` then `ApplyTypes.java` — put the structs and the global arrays (`g_human`, `g_object`...) into the Ghidra project. Run both after each change to the structs or the globals. `analysis/024d/data_globals.csv` holds globals with a base type (`float`, `int`, `char*`...) and a count.
- `tools/ghidra/InferFields.java` then `tools/re/infer_fields.py [--test]` — read the type of each field of the typed globals from all the instructions that use it (x87 = float/double, `jl` after `cmp` = signed...). It writes `analysis/structs/024d/<global>.csv`. `gen_types.py` makes typed `unk_` fields from it for the structs that have no Mac file. A piece without a field becomes an unsigned int in C, which breaks floats and signed compares: run this after you add a struct.
- `tools/ghidra/FixIntLocals.java` — after `ApplyTypes.java`, run it **once**: float locals that no float op uses become `int`, plus the locals of `analysis/024d/locals.csv` (each one checked by hand). A small integer in a float local is a denormal on the x87 and makes the code very slow. Do not run it twice (each run changes how the decompiler merges locals).
- `tools/ghidra/ApplyNames.java` — gives the functions their names from `analysis/024d/functions.csv` (only `FUN_` names). Run it after each change to the CSV.
- `tools/ghidra/ApplyParams.java` — gives parameters their types from `analysis/024d/params.csv` (a pointer typed as `int` makes the harness skip the call). Run it after each change to the CSV.
- `tools/ghidra/LiftExport.java` — export for the lifter. The p-code rules (float bits, rounding, relocated addresses, struct pieces) live here.

### Lift and test

- `tools/lift/lift.py --compile` — lifts the pseudo-C to `src/lifted/lifted_024d.c`. See `docs/lift.md`.
- `tools\build_hook.bat` — builds the hook DLL and the harness.
- `build/hook/harness.exe <024d_fixed.exe> <sv_snapshot.bin> <sv_args.txt> [filter]` — runs each recorded call with the lifted and the original version and compares them. `HARNESS_DEBUG`, `HARNESS_PROBE` and `HARNESS_WATCH` help to find a difference (`docs/lift.md`). Side-effect functions run in the test environment of `src/platform/plat_test.c` (`docs/side-effects.md`).
- `build/hook/subvulpes-server.exe` — the standalone server (Phase 5, `src/standalone/`). Run it in a server folder. `SV_SAMPLE=<file>` writes the samples of a 1 ms sampling profiler. `python -I tools/lift/samples.py <file>` counts them by function.
- `tools/launcher/build.bat` — builds `build/launcher/subvulpes-launcher.exe`, the server panel for the testers (C#, the compiler of .NET Framework 4 that comes with Windows, 32-bit for the dumps). Put it next to `subvulpes-server.exe`. A preset (Normal Playtest, Harness Recording, Playtest (For Speed Issues), Crashhunt, Custom) chooses the captures. Each test goes to `logs/<time>/` (`server.log`, `profile.txt`, `packets.bin`, dumps, `marks.txt`, `session.ini`, `notes.txt`), and "Pack" makes `logs/test-<time>.zip`. Ctrl+F12 marks the moment. An update zip (dropped on the window or on the exe) replaces only the exe files, `LEIA-ME.txt` and `version.txt`, with a backup. `launcher.ini test_crash=<s>` tests the crash path.
- `src/platform/plat_imports.c` — the imports of the game for the standalone server (static CRT, KERNEL32, ws2_32 and three functions of `WSOCK32.dll`). Add a new import here. `build/hook/crt_diff.exe` (`src/tests/crt_diff.c`) compares a CRT function of `MSVCR100.dll` with the new CRT before it moves.
- `build/hook/subvulpes-server-record.exe` — the harness recording build of the standalone server (`src/standalone/record_shim.c`). The lifted code has `/Gh`: `_penter` gives each call to the recorder of the hook build (`hook/record.c`, `hook/record_imports.c`), and a side-effect function (`lf_table` side=1) returns through `rec_return` to close its record. The image is at `0x20000000`, as `024d_fixed.exe`, so `harness.exe build/hook/024d_fixed.exe <dir>/sv_snapshot.bin <dir>/sv_args.txt` reads the output with no change. `SV_RECORD_DIR` (output folder), `SV_SNAPSHOT_AFTER` (seconds, default 600). About 57 ticks/s. The launcher preset Harness Recording uses it.
- Server switches (environment): `SV_LOG` (copy of the console), `SV_SAMPLE` (profile), `SV_TIMERS`, `SV_PACKETS` (the format of `hook/capture.c`, read by `tools/net/capture.py`), `SV_DUMP` and `SV_DUMP_TYPE=small|full` (minidump at a crash), `SV_TEST_CRASH=<s>`.
- `tools/lift/lint.py` — float/int conversions and uninitialized variables in the lifted C.
- `bash tools/lift/pipeline.sh [--params] [--no-gate] [--full] [gate options]` — LiftExport, lift, build and gate in one command. The gate is `--changed --quick` unless you give `--full`.
- `python -I tools/lift/gate.py [--changed] [--quick] [--jobs N] [--update] [--target NAME]` — harness on every recording of a target (`tools/lift/targets/ded024d.json`), compared function by function with a local baseline. It fails when a "calls same" count drops. `--changed` runs only the functions whose lifted C changed. `--quick` runs the fewest recordings that test every function of the baseline. `--all-calls` runs every recorded call (own baseline in `build/gate/ded024d/all`).
- `python -I tools/lift/min_calls.py [--keep 2] [--reuse]` — runs the harness in coverage mode (`HARNESS_COVER`, an INT3 on each basic block from `tools/ghidra/ExportBlocks.java`) and writes `sv_args.min.txt` for each recording: 2 calls for each code path, counted over all recordings. The full gate went from about 36 min to about 3 min with the same 284 tested functions.
- `python -I tools/lift/deps.py [--func NAME]` — Phase 4 progress: globals, calls and `SR_VA` that the lifted C still takes from the image.

### Ghidra extensions (installed in `E:\tools\ghidra_12.1.4_PUBLIC\Ghidra\Extensions`)

- **GhidraMCP** (LaurieWired, 2.0) — an MCP server that lets Claude decompile, rename and retype in Ghidra directly. It runs as its own process with the HTTP transport: `powershell -File tools\ghidra\mcp_server.ps1 start|stop|restart|refresh|status` (port 8081, log in `build/ghidra-mcp.log`). `.mcp.json` (not versioned, template in `tools/ghidra/mcp.json.example`) points to `http://127.0.0.1:8081/mcp`. The server opens a copy of the project. Run `bash tools/ghidra/mcp_refresh.sh` after each change to the real project (not after a change to `LiftExport.java` or `lift.py`). The copy opens with `--discard`: changes there are lost, so put permanent changes into a script.
- **ghidra-delinker-extension** (boricj, 0.9.2) — exports parts of the 024d as COFF object files with relocations (Phase 4). Run the "Relocation table synthesizer" analyzer first. Headless script: `DelinkProgram.java`.

## Plan

See `docs/plano.md`.
