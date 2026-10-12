# CLAUDE.md

RE/decompilation project for Sub Rosa (2015).

**Language:** everything in the repository is in English (code, comments, docs, commit messages). Chat with the user is in Portuguese (BR). Old commits stay in Portuguese.

## Rules

- Never version anything in `original/` except `README.md` and `MANIFEST.sha256`. Binaries and assets are proprietary.
- `original/` is read-only. Tools read from it and write to `analysis/`, `build/` or the scratchpad.
- Run Python with `-I` when reading files from `original/` or `third_party/`.
- The servers' `config.txt` has passwords. Do not copy its content into docs or commits.

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

## Plan

See `docs/plano.md`.
