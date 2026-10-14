# CLAUDE.md

RE/decompilation project for Sub Rosa (2015).

**Language:** everything in the repository is in English (code, comments, docs, commit messages). Chat with the user is in Portuguese (BR). Old commits stay in Portuguese.

## Rules

- Never version anything in `original/` except `README.md` and `MANIFEST.sha256`. Binaries and assets are proprietary.
- `original/` is read-only. Tools read from it and write to `analysis/`, `build/` or the scratchpad.
- Run Python with `-I` when reading files from `original/` or `third_party/`.
- The servers' `config.txt` has passwords. Do not copy its content into docs or commits.
- Recordings (`build/recordings/`) hold player names, IP addresses and `config.txt`. Never commit them or copy them into docs.
- **Fix the lifter in Ghidra, not in text.** Never add a regex rule to `tools/lift/lift.py`. When the lifted C is wrong, find what Ghidra does not know (a type, a struct field, an array global, a function pointer) and add it to the Ghidra project with a script, or write a p-code rule in `tools/ghidra/LiftExport.java`. The only text exception is `LIFT_PATCHES` in `lift.py`: one explicit fix for one decompiler bug in one function.
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
- `tools/re/struct_fields.py` — collects struct fields from the pseudo-C.
- `tools/formats/subrosa_formats.py` — readers for `.srk`, `.sbl`, `.sbb`, `.sbc`, `.csm`.

### Ghidra database (types and names)

- `tools/re/gen_types.py` — generates `src/include/sr_types.h` from `analysis/structs/names.csv`. Add a known field there, never in the header.
- `tools/ghidra/types_json.py` then `ApplyTypes.java` — put the structs and the global arrays (`g_human`, `g_object`...) into the Ghidra project. Run both after each change to the structs or the globals.
- `tools/ghidra/ApplyNames.java` — gives the functions their names from `analysis/024d/functions.csv` (only `FUN_` names). Run it after each change to the CSV.
- `tools/ghidra/LiftExport.java` — export for the lifter. The p-code rules (float bits, rounding, relocated addresses, struct pieces) live here.

### Lift and test

- `tools/lift/lift.py --compile` — lifts the pseudo-C to `src/lifted/lifted_024d.c`. See `docs/lift.md`.
- `tools\build_hook.bat` — builds the hook DLL and the harness.
- `build/hook/harness.exe <024d_fixed.exe> <sv_snapshot.bin> <sv_args.txt> [filter]` — runs each recorded call with the lifted and the original version and compares them. `HARNESS_DEBUG`, `HARNESS_PROBE` and `HARNESS_WATCH` help to find a difference (`docs/lift.md`). Side-effect functions run in the test environment of `src/platform/plat_test.c` (`docs/side-effects.md`).
- `tools/lift/lint.py` — float/int conversions and uninitialized variables in the lifted C.

### Ghidra extensions (installed in `E:\tools\ghidra_12.1.4_PUBLIC\Ghidra\Extensions`)

- **GhidraMCP** (LaurieWired, 2.0) — an MCP server that lets Claude decompile, rename and retype in Ghidra directly. It runs as its own process with the HTTP transport: `powershell -File tools\ghidra\mcp_server.ps1 start|stop|restart|refresh|status` (port 8081, log in `build/ghidra-mcp.log`). `.mcp.json` (not versioned, template in `tools/ghidra/mcp.json.example`) points to `http://127.0.0.1:8081/mcp`. The server opens a copy of the project. Run `bash tools/ghidra/mcp_refresh.sh` after each change to the real project (not after a change to `LiftExport.java` or `lift.py`). The copy opens with `--discard`: changes there are lost, so put permanent changes into a script.
- **ghidra-delinker-extension** (boricj, 0.9.2) — exports parts of the 024d as COFF object files with relocations (Phase 4). Run the "Relocation table synthesizer" analyzer first. Headless script: `DelinkProgram.java`.

## Plan

See `docs/plano.md`.
