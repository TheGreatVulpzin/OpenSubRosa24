# OpenSubRosa24

[![CI](https://github.com/TheGreatVulpzin/OpenSubRosa24/actions/workflows/ci.yml/badge.svg)](https://github.com/TheGreatVulpzin/OpenSubRosa24/actions/workflows/ci.yml)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
![Language: C](https://img.shields.io/badge/language-C-A8B9CC?logo=c&logoColor=white)
![Platform: Windows x86](https://img.shields.io/badge/platform-Windows%20x86-lightgrey)
![Decompiled with Ghidra](https://img.shields.io/badge/decompiled%20with-Ghidra-red)
![Last commit](https://img.shields.io/github/last-commit/TheGreatVulpzin/OpenSubRosa24)
![Version: 0.24 forever](https://img.shields.io/badge/version-0.24%20forever-ff69b4)

**(EN-US)**
The Sub Rosa 0.24 game (server and, in the future, client), decompiled with Ghidra and rewritten as readable C.
It is bit-exact with the original and compatible with original clients and servers. It also adds security fixes and, in the future, QoL changes.

> Status: the server is rewritten in C and almost fully documented. The client decomp is not started yet.

**(PT-BR)**
O jogo Sub Rosa 0.24 (servidor e, futuramente, client), decompilado com Ghidra e reescrito em C legível.
É bit-exact com o original e compatível com clients e servidores originais. Também inclui fixes de segurança e, futuramente, QoL.

> Status: o servidor está reescrito em C e praticamente documentado. O decomp do client ainda não foi iniciado.

## AI Disclaimer / Aviso de IA
**(PT-BR)**
Esse repositório foi feito com a ajuda de OpenAI Codex e Claude Code para a escrita do código, decompilamento e montagem de instrumentação. Apesar disso, vou falar que eu tive que ficar orientando muito a IA pra fazer o que eu queria...

**(EN-US)**
This repository was created with the help of OpenAI Codex and Claude Code for writing the code, decompiling, and setting up instrumentation. That said, I have to admit that I had to give the AI a lot of guidance to get it to do what I wanted...

## Repository layout

| Folder | Content |
| --- | --- |
| `src/` | The server in C: `common/` (math, files, parser), `game/` (humans, items, objects, level, economy), `net/` (packets, events, commands), `server/` (main loop, accounts), `platform/` (system calls), `include/` (generated types). |
| `src/lifted/`, `src/standalone/` | Runtime code shared by the server targets, including the image region and function tables. The server build needs these files. |
| `instrumentation/` | Hooks, the recorder, the harness, the test platform, the lifter, the gate, Ghidra scripts, and recording tools. The separate recording target uses this folder. |
| `docs/` | How each system works (`docs/game/`), the network protocol, file formats and the plan. |
| `analysis/` | Data taken from the original game binaries: function names, struct fields, globals. |
| `tools/` | Scripts for names, types and file formats. |
| `third_party/` | Tools of other people, kept for reference (the server manager). |
| `original/` | Your copy of the original game. See `original/README.md`. |

## Build

If you want to build the repository, you will need Windows and these tools:

- Microsoft C compiler for x86 (Visual Studio Build Tools). The project is built with MSVC 14.50 (VS 2026 Build Tools) and Windows SDK 10.0.26100.0.
- CMake.
- Python 3 (3.14 is used). The scripts have no external packages.

Open a "x86 Native Tools" prompt, or set `VCVARS` to the path of your `vcvars32.bat`. Then run:

```bat
tools\build_hook.bat
```

The build is x86 (32 bit). The result is in `build\hook\`:

| File | What it is |
| --- | --- |
| `subvulpes-server.exe` | The standalone server. It runs with no `subrosadedicated024d.exe`. |
| `subvulpes-server-record.exe` | The same server with a recorder, to make test recordings. |
| `wsock32.dll` | A hook DLL for the original server. Only for testing. |
| `harness.exe` | Compares each function with the original on recorded calls. |

## Run the server

If you just want to run the server, download the [server package](https://github.com/TheGreatVulpzin/OpenSubRosa24/releases).
The package includes `data/` and a default `config.txt`.

1. Extract the zip into a new folder.
2. Open `subvulpes-launcher.exe`.
3. Open **Server settings**. Set the server name, player limit, round time, UDP port and admin password. Click **Save**.
4. Open **Server** and click **Start**. The panel shows live `SV_LOG` output, players and tick speed.
5. Join with your Sub Rosa 0.24 client. Use **Stop**, or close the launcher, to stop the server safely.

The default admin password is empty. The launcher saves the password only in `config.txt`. Use **Show** to reveal or mask it.
Keep that file private. If it is missing, the launcher creates the package default.
The first-run message lists missing `data/` or `config.txt` on one line.

Open **Launcher settings**. Use **Check for updates** to find a newer GitHub release. The launcher checks the zip SHA-256 before installation.
Enable **Include pre-releases** in Launcher settings to include preview versions. Updates keep your configuration, data and accounts.
**Save Log** exports only the current `server.log` as a text file. It creates no zip.
It excludes configuration, packet captures, dumps, notes, recordings and version metadata.

Launcher settings also holds **Server folder**, **Browse** and **Debug Mode**.
The launcher starts with its own folder and remembers another folder if you select one.
Debug Mode shows the **Debug** tab and uses its selected preset on the next server start.
Debug keeps all five presets, capture controls, past tests and **Pack this test**.
It is off by default. **Ctrl+Shift+D** toggles it. `developer=1` in `launcher.ini` also enables it.

For a local build, package the approved data with `tools/release/make_package.ps1 -DataDir original\blackhouser\data -Version v0.0.2-test`.
See [Server releases](docs/release.md) for build and publication commands.
Set `SV_LOG` to a file path if you need a server log.


## Verify

```bash
python -I tools/re/manifest.py check
bash instrumentation/lift/pipeline.sh
```

The first command checks the hashes of your game files. The second builds and runs the quick gate.
The previous command, `bash tools/lift/pipeline.sh`, forwards to the same pipeline.

## Related projects

- **SubVulpes Server** (not published yet): a server base with plugins, admin tools and more!
- **OverRosa** (not published yet): An modified client for enhanced features, custom assets and more!

## Contributing

Bug reports and pull requests are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md). For security problems, read [SECURITY.md](SECURITY.md).

## License

The code written for this project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE).
This license covers only the code written for this project. It does not cover the Sub Rosa game. Its assets and binaries belong to the original creators.

## Legal notice

Sub Rosa is owned by Cryptic Sea. This repository does not contain original binaries or assets.
You need your own copy of the game to use some tools.
This project is not made by or linked to Cryptic Sea.
