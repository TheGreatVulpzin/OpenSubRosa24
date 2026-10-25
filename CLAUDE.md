# CLAUDE.md

RE/decompilation project for Sub Rosa (2015).

**Language:** everything in repo in English (code, comments, docs, commit messages). Chat with user in Portuguese (BR). Old commits stay Portuguese.

**Comments:** ASD-STE100 style (short sentences, active voice, simple words). No emojis. No dashes as punctuation (use period or colon). Keep short: say why, not what code already shows.

## Rules

- Verification tools live in `instrumentation/`: `hook/`, `tests/`, `platform/plat_test.*`, `standalone/record_shim.c`, `lift/`, `ghidra/`, and `recording/`.
- Both server targets need `src/lifted/`, `src/standalone/image_024d.*`, and `src/standalone/fn_table.c`. Keep these runtime files in the server build.
- Only the separate `subvulpes-server-record` target uses the recorder in `instrumentation/`. The normal server has no include path to that folder.
- The lifter generates `src/lifted/lifted_table.c` and the hook tables in `instrumentation/hook/`. Never edit these outputs by hand.
- `tools/build_hook.bat` still builds all targets into `build/hook/`. `tools/lift/pipeline.sh` forwards to `instrumentation/lift/pipeline.sh`.

- Never version anything in `original/` except `README.md` and `MANIFEST.sha256`. Binaries and assets proprietary.
- C tables generated from 024d `.rdata` and initial `.data` (strings, constants, default values) may be versioned in `src/` (Phase 5.1). User checked them.
- `original/` read-only. Tools read from it, write to `analysis/`, `build/` or scratchpad.
- Run Python with `-I` when reading files from `original/` or `third_party/`.
- Servers' `config.txt` has passwords. Never copy content into docs or commits.
- Recordings (`build/recordings/`) hold player names, IP addresses and `config.txt`. Never commit them or copy into docs.
- **Ghidra not in daily loop of server.** Rule is for server (Phase 6). Phase 7 (client) uses full method again: Ghidra, lifter, names. Phase 6 rewrites each function by hand into clean C. Lifted C = fallback and reference for rewrite. New name goes into `analysis/structs/names.csv` (and `tools/re/gen_types.py` for custom type), then `gen_types.py`, in seconds. Lifted C has own frozen header (`src/lifted/sr_types_lifted.h`, through `lift_support.h`), so new name never breaks it. `gen_types.py` stops when a record has another size in the two headers. Do not run ApplyTypes for server: new export would give lifted C new names (then copy `sr_types.h` into frozen header again). Use Ghidra only for new facts (unknown function, call target, data layout).
- **Never fix lifted C with text rules.** Never add regex rule to `instrumentation/lift/lift.py`. Lifted C of a function wrong: rewrite it by hand, or find what Ghidra does not know and fix with Ghidra script. Only text exception: `LIFT_PATCHES` in `lift.py`, one explicit fix for one decompiler bug in one function.
- Hand-written function must give 0 harness differences and not be slower than original. Measure with profiler (`subvulpes.ini profile=1`). `src/server/main_loop.c` = example of code that works but is not well optimized.
- **Gate cost.** Gate runs short call files (`sv_args.min.txt`, few calls per code path, made by `instrumentation/lift/min_calls.py`). While rewriting a function: run `tools\build_hook.bat` and `gate.py --all-calls --funcs <name>` (2 to 4 min), plus synthetic tests (`instrumentation/lift/synth_bonds.py`, `synth_humans.py`) for paths no recording reaches. After rewrite moves a function out of lifted C, run `pipeline.sh` (lift, build, `gate.py --changed --quick`): reads export in `build/lift`, no Ghidra start. Before push, run full gate (`pipeline.sh --full` or `gate.py`, about 3 min). Run `gate.py --all-calls` (every recorded call, 30 to 40 min) only now and then, e.g. weekly. After new recording, run `min_calls.py` and `gate.py --update`.
- **Wait in background.** Run every command over 1 min with `run_in_background`, then stop. System sends notification at end. No `sleep` loops, `until` loops or repeated log reads. No monitor only to watch another task.
- **Synthetic item tests.** `instrumentation/lift/synth_items.py --run <recording dir> <work dir>` builds 76 situations (guns, phones, bandages, doors, loose items, hands, bullets), runs harness on each. Case snapshot about 120 MB, so `--run` deletes it after run. Never generate all cases without `--run`: disk fills. `--no-cover` skips cover run. All cases must stay "same".
- Every lifter change must keep harness results: no "calls same" count may drop on any recording (see `docs/lift.md`).
- Knowledge that must stay goes into repo (`analysis/*.csv`, `src/include/sr_types.h`, scripts), never only Ghidra project. Ghidra project not versioned.

## Rewrite workflow (Phase 6)

One group of `docs/plano.md` ("The groups of the rewrite") per session. Split large group (4, 8) into steps of about 15 KB.

Two session kinds do a group. Opus session writes group plan to `build/plans/group<N>.md`, writes no code. Several plan sessions may run at once (2 or 3, usage limit). Sonnet session then does steps 2 to 6 from plan file. Fact missing from plan (bug in clean helper, shared helper, name): write it into plan file. Before adding a name, look for that offset in `names.csv`: another group may already name it.

1. Plan: per function, read lifted C (`git show HEAD:src/lifted/lifted_024d.c`, marker `/* --- 0x<rva> */`). Write down offsets, rounding points, NaN compare forms, dead code, locals the code reuses.
2. Names: add each field the clean code uses to `analysis/structs/names.csv` (or `tools/re/gen_types.py` for custom type), then run `gen_types.py`.
3. Write clean C in `src/game`, `src/common` or `src/net`. Remove the `lf_XXXXX` alias of the function from its callers.
   - **Comments describe code only.** Say what code does and why, short STE sentences. Never write about process: no "the original", "as the original", "the 024d", "lifted C", "not rewritten yet", "by hand", no phase, no harness history. Describe behavior itself. Example: write "the list has no size limit", not "the list has no size limit in the original". Call to function of another file gets comment "Game functions of other files.".
   - Code keeps quirk on purpose (local keeps old value, index past array): say quirk in plain words: what happens, for which input. Put reason it must stay (bit-exact result) in `docs/game/`, not in code.
   - Before each commit: `grep -n -i "original\|lifted\|rewritten\|024d" <the files you wrote>` must find nothing. Reviewer (step 5) checks too.
4. Run `bash instrumentation/lift/step.sh [--rec <recording dir>] [--synth <name>,...] <function>,...` in background. Runs `pipeline.sh`, `gate.py --all-calls --funcs` and synthetic tests, prints summary of few lines (logs in `build/step/`). Open log only when summary shows failure.
5. Get review from another model before each commit. Then run full gate (`gate.py`) before asking for push.
6. End of group: write or update doc of its system in `docs/game/` (how system works, records, functions). Mark group done in `docs/plano.md`.

End of a step (checklist). Do every item before reporting "done". Show proof of each item in report:

1. Run `bash instrumentation/lift/pipeline.sh`. Search its log for `error C` and `build failed`.
2. Run `git status`. After lift, `src/lifted/lifted_024d.c`, `src/lifted/lifted_table.c` and `instrumentation/hook/hooks_lifted.c` must show changes. Commit them with the step.
3. Run `bash instrumentation/lift/step.sh <functions>`. Summary must name each function of the step. Missing function = not tested.
4. Run `python -I instrumentation/lift/bench.py --funcs <functions of the step, plus the rewritten functions they call>`. Report the ratio and the calls of each. Above 1.05 with fewer than 20 calls = probably noise: run again with `--all-calls` (3 runs) before you change code. Above 1.05 with many calls in all runs = slower: fix it.
5. Run comment grep of step 3 on files you wrote.
6. Docs and `names.csv` must agree. Field meaning changed: `grep` its name in `docs/`, fix each hit.
7. Get review of step 5. Step with float math needs Opus.
8. After commit, `git status` must be clean. No push.

Session rules (plan and execution sessions):

- Tokens: use caveman skill at ultra level for own messages. Final report to user in Portuguese (BR). Use `cavecrew-investigator` for searches. Do not read whole file when plan gives offset or line.
- Execution session reads only two parts of its plan: Names section and section of current step. Opens lifted C only at points plan marks risky.
- Model per work kind: Haiku (subagent or session) for mechanical work: names batch, `gen_types.py`, `step.sh`, commits, push, docs from finished plan. Sonnet for transcription. Opus for plans and float debug.
- One plan step per commit.
- Before new name, read `names.csv` and shared notes other plan sessions left for this group.
- Measure speed (`instrumentation/lift/bench.py`) of each function that runs every tick. Clean code must not be slower.
- Review: Opus subagent for steps with float math, `cavecrew-reviewer` for other steps. Skip review of step with no float math (getters, tables, copies, names) when all-calls gate and synthetic tests of that step pass.
- Stop rule: gate shows float difference in last bits and two tries do not fix it: stop. Report function, bytes, checks done. User then moves debug to Opus.
- Ask before each push.

Traps:

- `pipeline.sh` exits 0 when build fails. Search its log for `error C` and `build failed`.
- In Git Bash, run build with `cmd //c "tools\\build_hook.bat"`. `cmd /c` does not run it.
- Lifted-style files in `src/game` (`objects.c`, `wheels.c`, `ai_car.c`) use frozen header. Do not change their `unk_` names.
- Do not write C with escapes (`\n`) through heredoc, `sed` or Python one-liner in Bash. Use Write or Edit tool.
- Rounding: store to memory rounds value to float, register does not. Lifted frame local (`*(float *)(lf_frame + n)`) = memory store. In clean C, put explicit `(float)` cast where original rounds. Value that stays in register in original (e.g. `1 / s` in `orientationtoquaternion`) stays `double`.
- Lifted C calls other functions through `SR_FN`, so uses original helpers. When caller becomes clean, it calls clean helpers, and hidden helper difference can show first time. Find with `HARNESS_WATCH` (writer of different byte), `HARNESS_PROBE` (values in original) and disassembly (`dumpbin /disasm:nobytes /range:<va>,<va>`).
- Game `rand`: call `os_rand()` (`src/include/sr_os.h`), never CRT `rand`.
- Structure: each original function stays one function, same name, parameters, bit-exact result. Inside, `static` helper may replace repeated code or split long function. Helper matches function of `subrosa.x64` (symbols): use that name. Value a helper returns is rounded to float: make sure original rounds at that point.

## Key binaries

- `original/client/subrosa.x64` — Linux ELF64 **with symbols**. Source of truth for function names.
- `original/client/subrosa.app/Contents/MacOS/subrosa` — Mach-O i386 with symbols. Reference for 32-bit layout.
- `original/blackhouser/subrosadedicated024d.exe` — PE32 without symbols. Target of server manager (SHA-256 `11e00678…`).
- 024d addresses always **RVAs** (relative to module base), as in server manager.

## Cloud session (Linux)

Cloud does heavy Ghidra work. MSVC build and game testing stay on Windows PC.

1. Run `bash tools/cloud/setup.sh`. Installs Java 21 and Ghidra 12.1.4, downloads official binaries from gart.sh. 5 to 10 min.
2. Run `bash instrumentation/ghidra/analyze_all.sh` to create Ghidra project. 10 to 30 min.
3. Commit only versionable results (`analysis/`, `docs/`, `src/`, `tools/`, `instrumentation/`). Never commit `original/`, `build/` or `ghidra-project/`.

Download fails: session needs network access to `crypticsea.gart.sh` and `github.com`.

## Tools

- `tools/re/strings.py <file> [min]` — ASCII strings.
- `tools/re/elf_symbols.py <elf> [--sections]` — ELF functions.
- `tools/re/survey_bins.py <folder> [--manifest …] [--tsv]` — format, symbols, SHA-256 of every executable in folder.
- `tools/re/manifest.py write|check` — hashes of `original/`.
- `instrumentation/ghidra/analyze_all.sh [name]` — imports and analyzes binaries. Paths in `instrumentation/ghidra/env.sh`.
- `instrumentation/ghidra/export_all.sh` — re-exports features without re-analyzing.
- `instrumentation/ghidra/decompile.sh <program> <output.c> [rva ...]` — pseudo-C. Program in project folder: `mac025/subrosa`.
- `tools/re/name_all.sh` — generates function-name CSVs.
- `tools/re/map_globals.py` — maps global variables by votes.
- `tools/re/map_regions.py` — maps unnamed globals to symbols by functions that use them (`--test` checks on known globals). Output = candidate list: check each before it goes into CSV.
- `tools/re/struct_fields.py` — collects struct fields from pseudo-C.
- `tools/formats/subrosa_formats.py` — readers for `.srk`, `.sbl`, `.sbb`, `.sbc`, `.csm`.

### Ghidra database (types and names)

- `tools/re/gen_types.py` — generates `src/include/sr_types.h` from `analysis/structs/names.csv`. Add known field there, never in header.
- `instrumentation/ghidra/types_json.py` then `ApplyTypes.java` — put structs and global arrays (`g_human`, `g_object`...) into Ghidra project. Run both after each change to structs or globals. `analysis/024d/data_globals.csv` holds globals with base type (`float`, `int`, `char*`...) and count.
- `instrumentation/ghidra/InferFields.java` then `tools/re/infer_fields.py [--test]` — read type of each field of typed globals from all instructions that use it (x87 = float/double, `jl` after `cmp` = signed...). Writes `analysis/structs/024d/<global>.csv`. `gen_types.py` makes typed `unk_` fields from it for structs with no Mac file. Piece without field becomes unsigned int in C, breaks floats and signed compares: run after adding a struct.
- `instrumentation/ghidra/FixIntLocals.java` — after `ApplyTypes.java`, run **once**: float locals no float op uses become `int`, plus locals of `analysis/024d/locals.csv` (each checked by hand). Small integer in float local = denormal on x87, very slow code. Never run twice (each run changes how decompiler merges locals).
- `instrumentation/ghidra/ApplyNames.java` — names functions from `analysis/024d/functions.csv` (only `FUN_` names). Run after each CSV change.
- `instrumentation/ghidra/ApplyParams.java` — types parameters from `analysis/024d/params.csv` (pointer typed as `int` makes harness skip the call). Run after each CSV change.
- `instrumentation/ghidra/LiftExport.java` — export for lifter. P-code rules (float bits, rounding, relocated addresses, struct pieces) live here.

### Lift and test

- `instrumentation/lift/lift.py --compile` — lifts pseudo-C to `src/lifted/lifted_024d.c`. See `docs/lift.md`.
- `tools\build_hook.bat` — builds hook DLL and harness.
- `build/hook/harness.exe <024d_fixed.exe> <sv_snapshot.bin> <sv_args.txt> [filter]` — runs each recorded call with lifted and original version, compares. `HARNESS_DEBUG`, `HARNESS_PROBE`, `HARNESS_WATCH` help find a difference (`docs/lift.md`). Side-effect functions run in test environment of `instrumentation/platform/plat_test.c` (`docs/side-effects.md`).
- `build/hook/opensubrosa24_server.exe` — standalone server (Phase 5, `src/standalone/`). Run in a server folder. `SV_SAMPLE=<file>` writes samples of 1 ms sampling profiler. `python -I instrumentation/lift/samples.py <file>` counts them by function.
- `tools/launcher/build.bat` — builds `build/launcher/subvulpes-launcher.exe`, server panel for testers (C#, .NET Framework 4 compiler that comes with Windows, 32-bit for dumps). Put next to `opensubrosa24_server.exe`. Preset (Normal Playtest, Harness Recording, Playtest (For Speed Issues), Crashhunt, Custom) chooses captures. Each test goes to `logs/<time>/` (`server.log`, `profile.txt`, `packets.bin`, dumps, `marks.txt`, `session.ini`, `notes.txt`), "Pack" makes `logs/test-<time>.zip`. Ctrl+F12 marks the moment. Update zip (dropped on window or exe) replaces only exe files, `LEIA-ME.txt`, `version.txt`, with backup. `launcher.ini test_crash=<s>` tests crash path.
- `src/platform/plat_imports.c` — game imports for standalone server (static CRT, KERNEL32, ws2_32, three functions of `WSOCK32.dll`). Add new import here. `build/hook/crt_diff.exe` (`instrumentation/tests/crt_diff.c`) compares a CRT function of `MSVCR100.dll` with new CRT before it moves.
- `build/hook/subvulpes-server-record.exe` — harness recording build of standalone server (`instrumentation/standalone/record_shim.c`). Lifted code has `/Gh`: `_penter` gives each call to recorder of hook build (`instrumentation/hook/record.c`, `instrumentation/hook/record_imports.c`), side-effect function (`lf_table` side=1) returns through `rec_return` to close its record. Image at `0x20000000`, as `024d_fixed.exe`, so `harness.exe build/hook/024d_fixed.exe <dir>/sv_snapshot.bin <dir>/sv_args.txt` reads output unchanged. `SV_RECORD_DIR` (output folder), `SV_SNAPSHOT_AFTER` (seconds, default 600). About 57 ticks/s. Launcher preset Harness Recording uses it.
- Server switches (environment): `SV_LOG` (console copy), `SV_SAMPLE` (profile), `SV_TIMERS`, `SV_PACKETS` (format of `instrumentation/hook/capture.c`, read by `tools/net/capture.py`), `SV_DUMP` and `SV_DUMP_TYPE=small|full` (minidump at crash), `SV_TEST_CRASH=<s>`.
- `instrumentation/lift/lint.py` — float/int conversions and uninitialized variables in lifted C.
- `bash instrumentation/lift/pipeline.sh [--export] [--params] [--no-gate] [--full] [gate options]` — lift, build, gate in one command. `--export` runs LiftExport in Ghidra first (without it, lift.py reads `build/lift/ded024d.jsonl`). Gate = `--changed --quick` unless `--full`. Script stops when build fails.
- `python -I instrumentation/lift/gate.py [--changed] [--quick] [--jobs N] [--update] [--target NAME]` — harness on every recording of a target (`instrumentation/lift/targets/ded024d.json`), compared per function with local baseline. Fails when a "calls same" count drops. `--changed` runs only functions whose lifted C changed. `--quick` runs fewest recordings that test every function of baseline. `--all-calls` runs every recorded call (own baseline in `build/gate/ded024d/all`).
- `python -I instrumentation/lift/min_calls.py [--keep 2] [--reuse]` — runs harness in coverage mode (`HARNESS_COVER`, INT3 on each basic block from `instrumentation/ghidra/ExportBlocks.java`), writes `sv_args.min.txt` per recording: 2 calls per code path, counted over all recordings. Full gate went from about 36 min to about 3 min with same 284 tested functions.
- `python -I instrumentation/lift/deps.py [--func NAME]` — Phase 4 progress: globals, calls, `SR_VA` lifted C still takes from image.

### Ghidra extensions (installed in `E:\tools\ghidra_12.1.4_PUBLIC\Ghidra\Extensions`)

- **GhidraMCP** (LaurieWired, 2.0) — MCP server: Claude decompiles, renames, retypes in Ghidra directly. Own process, HTTP transport: `powershell -File instrumentation\ghidra\mcp_server.ps1 start|stop|restart|refresh|status` (port 8081, log in `build/ghidra-mcp.log`). `.mcp.json` (not versioned, template `instrumentation/ghidra/mcp.json.example`) points to `http://127.0.0.1:8081/mcp`. Server opens a copy of project. Run `bash instrumentation/ghidra/mcp_refresh.sh` after each change to real project (not after change to `LiftExport.java` or `lift.py`). Copy opens with `--discard`: changes there lost, so put permanent changes into a script.
- **ghidra-delinker-extension** (boricj, 0.9.2) — exports parts of 024d as COFF object files with relocations (Phase 4). Run "Relocation table synthesizer" analyzer first. Headless script: `DelinkProgram.java`.

## Plan

See `docs/plano.md`.
