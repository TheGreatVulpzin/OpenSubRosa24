# Initial inventory

Survey made on 2026-10-11, with static analysis only (nothing was executed).
Full hashes in [`original/MANIFEST.sha256`](../original/MANIFEST.sha256).

## Binaries

| File | Format | Symbols | SHA-256 (start) | Note |
| --- | --- | --- | --- | --- |
| `client/subrosa.x64` | ELF64 x86-64, Linux, build 2015-08-02 | **Yes (not stripped)** | `9afeea22` | Best target for naming functions. 2757 functions, ~817 from the game. |
| `client/subrosa.app/.../MacOS/subrosa` | Mach-O **i386** | Yes (`_gameloop`, `_createhuman`...) | `78119827` | 32-bit like Windows: helps match x86 struct layouts. |
| `client/subrosa.exe` | PE32 i386, GUI | No | `294db16c` | Windows client. Steam (`steam_api.dll`). |
| `client/subrosadedicated.exe` | PE32 i386, console | No | `54f18a19` | Dedicated server that came with the client. |
| `blackhouser/subrosadedicated024d.exe` | PE32 i386, console | No | `11e00678` | **0.24d** dedicated server (2015-09-04). It is the target of the server manager. |
| `client/wsock32.dll` | PE32 DLL proxy (2024) | — | `99d3fada` | Third-party mod loaded by the client (strings: `conveniencestore1`, `texture/cash_thousand.png`). Needs analysis. |

> Hypothesis to confirm: the client is the same version (0.24) as the 024d dedicated server.
> Compare equivalent functions (e.g. `serverrecv`) between the ELF and the PE.

## What the ELF symbols show

- **Procedural C** code (only 20 C++ `_Z` symbols). Lowercase names, no prefix: `gameloop`, `humansimulation`, `serverrecv`.
- Embedded libraries: GLee (OpenGL), stb_image, zlib, Mersenne Twister (`genrand_*`), Speex. External: SDL2, OpenAL, Steam.
- `.bss` has **~731 MB**: huge static pools (players, bodies, vehicles, items). A direct problem for the web port (WASM).
- Full list: [`analysis/symbols/subrosa_x64.functions.txt`](../analysis/symbols/subrosa_x64.functions.txt).

### Subsystems (by function prefix)

| Subsystem | Examples |
| --- | --- |
| Loop / input | `main`, `gameloop`, `dedicatedserverloop`, `getinputs_*`, `checkkeyboard` |
| Network | `serverrecv`, `serversend`, `servercycle`, `clientrecv`, `clientsend`, `pbitwrite*/pbitread*`, `packobjectpacket`, `createevent_*` |
| Master server | `getmasterserverip`, `getonlineserverlist` (HTTP to `www.crypticsea.com/anewzero/serverinfo.php`) |
| Humans / ragdoll | `createhuman`, `humansimulation` (21 KB), `human_calculatearmangles`, `bondsimulation2` (30 KB) |
| Vehicles | `objectsimulation_car`, `objectsimulation_heli`, `objecttrainsimulation`, `getenginetorque*` |
| Items / weapons | `createitem`, `itemsimulation`, `itemweaponsimulation`, `bulletsimulation` |
| Game logic | `logicsimulation_round/_world/_driving`, `logic_cardealer_buy`, `logic_gunstore_buy`, `economysimulation` |
| Scenarios / missions | `createscenario_*`, `createmission_acquisition/transaction/threeway` |
| Map / city | `area_*`, `building_*`, `city_*`, `landscape_*`, editors `leveleditor_*`, `buildingedit_*` |
| Render | `render*`, `drawhud`, `drawmenuitems`, `generatetexture_*` (procedural textures) |
| Voice | `encodespeex`, `decodespeex`, `calculateplayervoice` |
| Accounts | `createaccount`, `loadaccountsserver`, `accountdeathtax` |

## Game data

| Extension | Count | Likely content |
| --- | --- | --- |
| `.sbl` | 60 | Blocks (`data/block`) |
| `.sbb` | 58 | Buildings (`data/building/%s.sbb`) |
| `.sbc` | 2 | City (`data/city.sbc`, 1 MB) |
| `.csm` | 2 | Models (`loadmodelcsm`) |
| `.png` / `.wav` | 52 / 26 | Textures and sounds |
| `.glvs` / `.glfs` | 10 / 10 | GLSL shaders (vertex/fragment) |
| `.srk` | 2 | Server accounts/state (`server.srk`) |

## Black Houser server

- `subrosadedicated024d.exe` + `config.txt` (gametype 4, 24 players, 30-minute round).
- `server.srk` with saved accounts.

## Server Manager 0.7.1 (`third_party/server-manager`)

Made by a friend, with ChatGPT. Python + Tkinter, Windows only.

- Opens the 024d dedicated server process and applies **memory patches** (`WriteProcessMemory`), or injects native DLLs.
- Works only with the exact hash `11e00678…`. Addresses are fixed RVAs.
- JSON plugins (`plugins/`), converted from Cheat Engine tables (`source_tables/*.CT`).
- Native DLLs in C (`native_sdk/`): black market, admin commands (`/heal`, `/tp`, `/npc`, `/imortal`...), death messages.
- **Value for the decomp:** `ANALISE-NPCS.md` and `ANALISE-BLACK-MARKET.md` already map RVAs, pools and struct offsets of the 024d:

| Pool (024d) | Record size | Capacity |
| --- | --- | --- |
| Player | `0x2408` | 256 |
| Human body | `0x2B64` | 512 |
| Vehicle | `0x8230` | 1024 |
| Connection | `0xF170` | — |

This information should be imported as labels in the RE project (see the plan).
