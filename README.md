# OpenSubRosa24

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
| `hook/`, `src/lifted/`, `src/tests/`, `tools/lift/` | Tools that check and prove that the rewritten code is equal to the original code: hooks, the lifter, the harness, the gate, and recording tools. (Not needed to run the server.) |
| `docs/` | How each system works (`docs/game/`), the network protocol, file formats and the plan. |
| `analysis/` | Data taken from the original game binaries: function names, struct fields, globals. |
| `tools/` | Scripts for names, types and file formats. |
| `third_party/` | Tools of other people, kept for reference (the server manager). |
| `original/` | Your copy of the original game. See `original/README.md`. |

## Build

You need Windows and these tools:

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

You need the server files from your own copy of Sub Rosa (the folder with `data/` and `config.txt`).

1. Copy `build\hook\subvulpes-server.exe` into that server folder.
2. Edit `config.txt` for the name, the player limit and the round time. Never share this file: it holds the admin password.
3. Start `subvulpes-server.exe` from that folder.
4. Open the game, and join the server with an original 0.24 client.

Optional: set the `SV_LOG` environment variable to copy the log to the console. `tools\launcher\build.bat` makes a small panel (`build\launcher\subvulpes-launcher.exe`). Put it next to the server to start it, pick a preset and pack the logs.

## Verify

```bash
python -I tools/re/manifest.py check
bash tools/lift/pipeline.sh
```

The first command checks the hashes of your game files. The second builds and runs the quick gate.

## Related projects

- **SubVulpes Server** (not published yet): a server base with plugins, admin tools and more.

## Contributing

Bug reports and pull requests are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md). For security problems, read [SECURITY.md](SECURITY.md).

## License

The code written for this project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE).
This license covers only the code written for this project. It does not cover the Sub Rosa game. Its assets and binaries belong to the original creators.

## Legal notice

Sub Rosa is owned by Cryptic Sea. This repository does not contain original binaries or assets.
You need your own copy of the game to use some tools.
This project is not made by or linked to Cryptic Sea.
