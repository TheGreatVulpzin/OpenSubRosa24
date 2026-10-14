# Environment setup

Reference environment: Windows 10, Git Bash and PowerShell.

## Tools

| Tool | Version | Where | Purpose |
| --- | --- | --- | --- |
| Python | 3.14 | `C:\Python314` | Scripts in `tools/`. No external dependencies. |
| Java (Temurin) | 21.0.7 | `C:\Program Files\Eclipse Adoptium\jdk-21.0.7.6-hotspot` | Ghidra requirement. |
| Ghidra | 12.1.4 | `E:\tools\ghidra_12.1.4_PUBLIC` | Decompiler. SHA-256 of the zip: `ddac49f9…d2d4db`. |
| VS 2026 Build Tools | MSVC 14.50 | `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools` | C compiler for x86 (hook DLL). |
| Windows SDK | 10.0.26100.0 | `C:\Program Files (x86)\Windows Kits\10` | Windows headers and libs. |
| CMake | — | `C:\Program Files\CMake` | Build of the code in `src/` and `hook/`. |
| GhidraMCP | 2.0 (LaurieWired) | Ghidra extension + `E:\tools\ghidramcp-venv` | MCP server: Claude uses Ghidra directly. |
| Python 3.10 | 3.10 | Windows Store | Only for the GhidraMCP venv (PyGhidra needs 3.10 to 3.13). |
| ghidra-delinker-extension | 0.9.2 (boricj) | Ghidra extension | Exports parts of the 024d as COFF object files. |
| GitHub CLI | 2.102.0 | `C:\Program Files\GitHub CLI` | CI status (`gh run list`). Log in once with `gh auth login`. |

The C: drive has little free space. For this reason Ghidra, the Ghidra project (`ghidra-project` is a junction to `E:\subvulpes\ghidra-project`) and the recordings live on E:.

## Ghidra extensions

Both extensions come from their GitHub releases for Ghidra 12.1.4. Unpack each release zip into `E:\tools\ghidra_12.1.4_PUBLIC\Ghidra\Extensions\`.

- Delinker: `ghidra_12.1.4_PUBLIC_20260926_ghidra-delinker-extension.zip` from [boricj/ghidra-delinker-extension](https://github.com/boricj/ghidra-delinker-extension/releases) (SHA-256 `44c569cb…ad5765`).
- GhidraMCP: the inner `ghidra_12.1.4_PUBLIC_20261010_GhidraMCP.zip` of `GhidraMCP-release-2-0.zip` from [LaurieWired/GhidraMCP](https://github.com/LaurieWired/GhidraMCP/releases).

GhidraMCP also needs its Python bridge, in its own venv:

```bat
py -3.10 -m venv E:\tools\ghidramcp-venv
E:\tools\ghidramcp-venv\Scripts\python.exe -m pip install "ghidra_mcp-2.0.0-py3-none-any.whl[headless]"
```

Then copy `tools/ghidra/mcp.json.example` to `.mcp.json` at the repository root, fix the paths, and run `bash tools/ghidra/mcp_refresh.sh` to make the project copy that the server opens. Claude Code asks to approve the `ghidra` server at the start of the next session.

## Original files

1. Copy the game files to `original/` (see [original/README.md](../original/README.md)).
2. Check the hashes:

```bash
python -I tools/re/manifest.py check
```

## Ghidra: analysis and export

The script imports the binaries into the local project `ghidra-project/` (outside git). It exports the function features to `build/features/*.jsonl`. The first run takes 10 to 30 minutes.

```bash
JAVA_HOME="/c/Program Files/Eclipse Adoptium/jdk-21.0.7.6-hotspot" tools/ghidra/analyze_all.sh
```

To open the project in the GUI, run `E:\tools\ghidra_12.1.4_PUBLIC\ghidraRun.bat` and open `ghidra-project/subvulpes.gpr`.

## Types and names in the Ghidra project

After the analysis, put the structs and the global arrays into the project (run again after each change to `analysis/structs/names.csv` or the globals CSVs):

```bash
python -I tools/re/gen_types.py
python -I tools/ghidra/types_json.py
source tools/ghidra/env.sh
"$HEADLESS" "$PROJECT" subvulpes -process subrosadedicated024d.exe -noanalysis \
    -scriptPath "$ROOT/tools/ghidra" -postScript ApplyTypes.java "$(cygpath -w "$ROOT/build/ghidra/types.json")"
bash tools/ghidra/mcp_refresh.sh
```

## Function names

This command carries the names from the Linux ELF and the Mach-O over to the 024d server:

```bash
python -I tools/re/match_functions.py --target build/features/ded024d.jsonl --ref build/features/linux_x64.jsonl --ref build/features/mac_i386.jsonl --out analysis/024d/functions.csv
```

## Build for x86 with MSVC

Use the MSVC x86 environment in `cmd`:

```bat
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
```

The warning `'vswhere.exe' is not recognized` appears (it is localized on non-English Windows). It does not stop the build.
