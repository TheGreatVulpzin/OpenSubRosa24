# Third-party mods in the client

The group's client has fan-made optimizations. They live in DLLs that are not from Cryptic Sea.

## `client/wsock32.dll`

| Field | Value |
| --- | --- |
| SHA-256 | `99d3fadae0e3b24f0cb24c664fd5e84f5710bfa94a5e4d61a6d785fc483646dd` |
| Build | 2023-03-14, MSVC 2022 (linker 14.34), C++ |
| Type | Proxy DLL. Exports the 75 functions of the Windows `wsock32.dll`. |
| Decompilation | `build/decomp/wsock32_fan.c` (generated, outside git) |

### How it loads

1. `subrosa.exe` imports `WSOCK32.dll`. Windows loads the copy in the game folder first.
2. In `DllMain`, the DLL loads the real `wsock32.dll` from `GetSystemDirectoryW` and stores the 75 addresses to forward the calls.
3. The DLL installs a hook at `subrosa.exe+0x1ab0c` (game start). When the game passes there, the DLL applies all patches.
4. The DLL also creates an MT19937 generator seeded from `std::random_device`.

### Patches in `subrosa.exe`

The names come from `analysis/client_win/functions.csv` (automatic matcher). "?" marks an unnamed function.

| RVA | Function | Patch | Likely effect |
| --- | --- | --- | --- |
| `0x9c3d2` | `createlevel` | JMP to the DLL (9 bytes) | Changes the map creation. |
| `0xda051` | `?` (`0xd9fa0`) | JMP to the DLL | Large map/render function (uses `2052` and `24680`). |
| `0xe3c94` | `rendersprites` | JMP to the DLL | Changes sprite drawing. |
| `0x106324` | `setupperspectiveviewport` | JMP to the DLL | Changes the camera (FOV or screen aspect). |
| `0x7cdaf` | `gameloop` | 6 bytes of NOP | Removes one instruction from the main loop. |
| `0x7a859` | `resetgame` | 1 byte = 0 | Changes a constant when the match restarts. |
| `0xfafe0` | `resettimer` | RET | Turns the function off. |
| `0xfb020` | `starttimer` | RET | Turns the function off. |
| `0xfb040` | `?` (likely `stoptimer`) | RET | Turns the function off. |
| `0xd9fc9`, `0xd9fd0` | `?` (`0xd9fa0`) | Replaces 7 bytes each | Changes instructions in the same map/render function. |
| `0xda334`, `0xda3fb` | `?` (`0xd9fa0`) | `cmp [ebp-0x80], 0x20` and `0x10` | Changes two loop limits. |
| `0xe8456` to `0xe8482`, `0xe8760` to `0xe8774` | `generatetexture` | 9 replacements of 7 bytes | Changes the generation of procedural textures. |

The three `RET` patches turn off the timers that measure time (profiling). This is probably part of the "optimization".

### Intercepted functions (full hook)

| RVA | Function | Handler in the DLL | What it does |
| --- | --- | --- | --- |
| `0x79ad0` | `resetgame` | `FUN_10002db0` | If the game mode is 0, it uses its own mode (5). Removes players and bodies, edits map blocks, creates 10 vehicles. Looks like a custom scenario. |
| `0xac190` | `setupmodel` | `FUN_10004760` | Adds models (likely `conveniencestore1`). |
| `0x10ac40` | `setuptexture` | `FUN_10003370` | Adds textures (`texture/cash_thousand.png`). |

### Game functions the DLL calls

`createplayer` (`0xd4750`), `deleteplayer` (`0xd4a20`), `createhuman` (`0x850b0`), `createitem` (`0x96cf0`), `createobject` (`0xc5780`), `linkitem` (`0x99d50`), `scenario_armhuman` (`0xddaf0`), `area_marksector` (`0x39230`), `loadtexturesetalpha` (`0x10a2f0`), `generatetexture` (`0xe7ff0`).

Unnamed functions the DLL calls, with the likely identification:

| RVA | Evidence | Likely |
| --- | --- | --- |
| `0xc6ff0` | Uses `0x2B64` (body) and `0x8230` (object). The DLL calls it for all 512 slots. | `deletehuman` |
| `0x43c60` | Takes block coordinates. Uses `2052` and `24680`. | `area_setblock` |

### Risks for SubVulpes

| Risk | Assessment |
| --- | --- |
| Server | None. The DLL exists only on the client. |
| Network protocol | No patch touches the network functions. The modded client already plays on the original 024d. |
| Conflict with our hooks (Phase 6) | Real. Do not put our hooks at the addresses above while the DLL is active. |
| File name (Phase 6) | Our DLL is also named `wsock32.dll`. Rename the mod to `wsock32_fan.dll` and load it from ours. |

Security signs: the DLL imports only memory, DLL loading and CRT functions. It does not import internet, process, registry or file-writing functions. Conclusion: low risk.

## `client/SDL2.dll`

File dated 2024-02-21. The original game is from 2015. Someone replaced SDL2 with a newer version.

## Open items

- Read the handlers `FUN_10004760` and `FUN_10003370` and the jumps to `0x10002c3b`, `0x10002c8b`, `0x10002cab` and `0x10002d0b`.
- Find out when the game mode is 0 on the client (menu, offline or match).
- Find out the version of `SDL2.dll`.
