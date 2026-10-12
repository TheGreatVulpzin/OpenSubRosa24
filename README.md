# SubVulpes

Analysis and decompilation project for **Sub Rosa** (Cryptic Sea, 2015 build, server 0.24d).

Goals:

1. Understand the game code (client and dedicated server).
2. Recreate the code in C, compilable and equivalent to the original.
3. Improve the servers: performance, stability, admin tools, mods.
4. Add new features.
5. Port to other platforms (Linux, modern macOS, ARM) and to the browser (WebAssembly).

## Structure

| Folder | Content |
| --- | --- |
| `original/` | Original game files. **Not in git.** See [original/README.md](original/README.md). |
| `analysis/` | Data extracted from the binaries (symbols, tables, RE notes). |
| `docs/` | Documentation. Start with [docs/inventario.md](docs/inventario.md). |
| `tools/re/` | RE scripts with no dependencies (strings, ELF symbols, manifest). |
| `src/` | Decompiled / reimplemented code (still empty). |
| `third_party/server-manager/` | Server Manager 0.7.1 (memory patches for the 024d dedicated server). |

## Getting started

```bash
python -I tools/re/manifest.py check
```

```bash
python -I tools/re/elf_symbols.py original/client/subrosa.x64
```

## Legal notice

Sub Rosa is owned by Cryptic Sea. This repository does not distribute original binaries or assets.
You need your own copy of the game to use anything here.
