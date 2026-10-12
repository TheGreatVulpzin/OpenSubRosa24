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

The C: drive has little free space. For this reason Ghidra lives on E:.

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
