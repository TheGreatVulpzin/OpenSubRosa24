# SubVulpes plan — Sub Rosa decomp

## Context

The repository is already initialized (commit `b8ca658`): `original/` outside git with a SHA-256 manifest, the friend's server manager in `third_party/server-manager`, tools in `tools/re/`, inventory in `docs/inventario.md`.

Goal: decompile Sub Rosa to improve servers, add features, optimize and port (Linux, macOS, browser).

Decisions made with you:

- **Strategy:** functional decomp + hooks (re3/OpenRCT2 style). Each function is rewritten in C and replaces the original inside the real executable.
- **Start:** dedicated server.
- **Protocol:** compatible with the original 0.24 clients.
- **Repo:** private for now.

Technical facts that guide the plan:

| Fact | Consequence |
| --- | --- |
| `subrosa.x64` (Linux) and the i386 Mach-O **have symbols**. Build of 2015-08-02, same date as the client's `subrosa.exe` and `subrosadedicated.exe`. | Names of ~817 functions for free. The Mach-O gives the 32-bit struct layout. |
| All PE files use **MSVC 2010** (linker 10.0, `MSVCR100.dll`). | Hooks must respect the MSVC conventions. Future matching is possible with VS2010. |
| `subrosadedicated024d.exe` (2015-09-05) **imports `WSOCK32.dll`**. | A proxy `wsock32.dll` in the server folder loads our code without an injector. The group's client already uses this trick. |
| `.data` of the dedicated server is ~636 MB (static pools). | Must become dynamic allocation in our own server and, above all, in WASM. |
| The manager already mapped RVAs, pools and offsets of the 024d (`ANALISE-NPCS.md`, `ANALISE-BLACK-MARKET.md`, `native_sdk/*.h`). | Import them as initial labels. |
| Machine: Python 3.14, Java 21, CMake, git. **No C compiler.** | Install VS 2022 Build Tools (x86) and Ghidra. |

---

## Phase 0 — RE foundation (first thing to run)

1. Install tools: Ghidra 11.x (uses the existing Java 21), VS 2022 Build Tools with MSVC x86. Document in `docs/setup.md`.
2. Create a local Ghidra project (gitignored) with: `subrosa.x64`, i386 Mach-O, `subrosadedicated.exe` (Aug), `subrosadedicated024d.exe` (Sep).
3. Headless script `tools/ghidra/import_symbols.py`: applies the ELF/Mach-O names and propagates them to the PE files via Version Tracking (Aug → Sep are almost equal).
4. **Confirm the protocol version:** compare `serverrecv`/`clientrecv` and the version constant between the client (Aug) and the 024d. This tells whether the group's client talks to the 024d. Record it in `docs/versoes.md`.
5. Analyze the client's `wsock32.dll` (2024 mod): what it does and whether it is safe. Record it in `docs/mods-terceiros.md`.
6. Document file formats: `.sbc`, `.sbb`, `.sbl`, `.csm`, `.srk`, `config.txt`. File `docs/formatos.md` + parsers in `tools/formats/`.

**Output:** `analysis/024d/functions.csv` (RVA, size, name, name source, confidence) and `analysis/024d/globals.csv`. These CSVs are the versioned source of truth. The Ghidra project does not go into git.

## Phase 1 — Structs and globals

1. Build `src/include/sr_types.h` with the structs: `Player` (0x2408), `Human` (0x2B64), `Vehicle/Object` (0x8230), `Item`, `Connection` (0xF170), `Account`, `Event`. Use the i386 Mach-O as the layout reference.
2. `static_assert` for size and offset of each known field.
3. Script that exports the structs to Ghidra (`tools/ghidra/apply_types.py`), so the decompiler shows named fields.

## Phase 2 — Hook infrastructure

New layout in `src/`:

```
src/
  include/      sr_types.h, globals.h (macros GLOBAL(type, name, rva))
  common/       math, vectors, parser, MT19937, bitstream (pbitwrite/pbitread)
  game/         humans, items, objects, vehicles, logic, economy, scenarios
  net/          packets, events, servercycle/serverrecv/serversend
  server/       dedicatedserverloop, accounts, config
  platform/     sockets, timer, files (abstraction for Win/Linux/WASM)
hook/
  wsock32_proxy/  proxy DLL: forwards the 21 imports to the system wsock32
  hooks.c         HOOK(rva, function) table, install with a 5-byte JMP
```

Rules:

- `HOOK(0x50AE0, createhuman)` registers the replacement. A `hooks.cfg` turns each hook on/off, to bisect bugs.
- Globals access the original memory via `GLOBAL(Player, players[256], 0xEDEDD00)` until the server is standalone.
- Mandatory spike: check the MSVC 2010 calling conventions (`__cdecl`, `__fastcall`, custom LTCG registers). Create `naked` wrappers where needed.
- First hooks: leaf functions (vectors, `pbitwrite*`) to validate the pipeline end to end.

## Phase 3 — Server decomp, bottom up

Order: utilities → bitstream/packets → events (`createevent_*`) → players/accounts → items/objects/vehicles → humans (`humansimulation`, `bondsimulation2`) → logic/economy/scenarios → `servercycle` → `dedicatedserverloop`.

- `tools/progress.py` computes the % of functions and bytes replaced from the CSV. Publish it in `docs/progresso.md`.
- Each new function counts as done only after it passes the differential test (see Verification).

## Phase 4 — Standalone server (`subvulpes-server`)

Phase 3 is closed (2026-10-16, `docs/progresso.md`): 321 of 340 functions are in C (99.2 % of the code bytes), and the lifted C makes no call to the `.exe`. It still takes 1473 globals, 657 inline `SR_VA` and 123 import references from the image (`tools/lift/deps.py`).

1. **4.1: mirrored `.data`** (1 to 2 days). A new `subvulpes-server.exe` reserves a region with the layout of the 024d `.data` and sets `sr_base` (`src/lifted/lift_support.h`) to it. The lifted C does not change. A table maps the globals and `SR_VA` values that hold addresses of `.text` functions (`qsort` comparator, `freadptr`/`fwriteptr`) to the C functions.
2. **4.2: `.rdata` and initial `.data` in C** (about 1 day). A script generates C tables from the 024d: the `.rdata` strings and constants (7 KB) and the initial `.data` bytes (3.5 KB). The generated files are versioned in `src/`. The import names are not copied: the linker makes the import table.
3. **4.3: platform layer** (1 to 2 days). Sockets, timer and files go to `src/platform/`. Keep the x87 precision at 53 bits and the `rand()` of MSVCR100.
4. **In-game test** of `subvulpes-server.exe` with players. The 39 harness-only functions (`serverrecv`, `serversend`...) run live in C here for the first time. Look at them first when a network bug shows.
5. **4.5:** named globals in place of the mirrored `.data`, dynamic pools with configurable limits, Linux x64. Watch structs with pointers on 64 bits. SSE in place of x87 changes the float bits of more functions than `buildsomeshit`: decide the accepted precision when this step starts.

Verification: the harness keeps testing the lifted functions against the 024d. No packet replay test for now. Add it if a bug is too hard to find in game.

Stay 100% compatible with the 0.24 client.

## Phase 5 — Better server

1. Native plugin API (C, loaded by DLL/.so) in place of memory patches.
2. Port the manager modules as plugins: black market, admin commands (`/heal`, `/tp`, `/npc`, `/imortal`...), death messages, faster truck, starting money, punches, bots.
3. Admin console / RCON, structured logs, server-side anti-cheat checks.
4. Own server list if the Cryptic Sea master (`crypticsea.com/anewzero/serverinfo.php`) no longer answers.
5. Profile `servercycle` and optimize (tick rate, packet sending, collisions).
6. Fix the input checks of the original server (`docs/protocol.md`, section 12, item 15):
   - **Voice index (security):** `serverrecv` (0x33650) reads the voice frame index of a type 4 packet with 8 bits and uses it before the `& 0x3f` mask. An index from 64 to 255 makes `memcpy` write up to 255 client bytes up to 48 KB after the 64-entry voice ring. Mask or reject the index before the copy.
   - Item count of the object part (8 bits, no cap), the server name copy (`strcpy`, longer than the 32 bytes sent) and the human health (`health + 10` in 7 bits, wraps above 117).

## Phase 6 — Client

1. Same hook method in `subrosa.exe` (which also imports `WSOCK32.dll`).
2. Reuse `common/`, `game/`, `net/` from the server. Missing: render, input, audio, voice, menus, editors.
3. Standalone client with SDL2 + OpenAL. Replace the fixed-pipeline OpenGL with GL 3.3 / GLES 3 (prerequisite for WebGL2).

## Phase 7 — Ports

1. Linux, macOS (arm64), Windows x64 with the standalone client.
2. **Browser (Emscripten/WASM):**
   - Render in WebGL2 (comes from Phase 6.3).
   - Network: the browser cannot do UDP. The server gets a WebSocket (or WebRTC DataChannel) transport next to UDP.
   - Memory: dynamic pools from Phase 4.
   - Assets: the player loads their own copy of the game (file picker + IndexedDB). No hosted assets.

---

## Critical files

- `original/blackhouser/subrosadedicated024d.exe` — target of the server hooks.
- `original/client/subrosa.x64`, `.../MacOS/subrosa` — sources of names and layouts.
- `analysis/symbols/subrosa_x64.functions.txt` — already generated by `tools/re/elf_symbols.py`.
- `third_party/server-manager/ANALISE-*.md`, `native_sdk/*.h`, `core.py` — RVAs and offsets already mapped. `core.py` also shows how to validate the 024d hash.
- `third_party/server-manager/tools/verify_*.py` — use Unicorn to emulate x86. Reuse the idea for differential tests.

## Verification

- **Integrity:** `python -I tools/re/manifest.py check` before each RE session.
- **Differential test per function:** run the original (via Unicorn, as the manager does, or by calling the original inside the process) and the rewrite with the same inputs and memory. Compare the return value and the memory written.
- **In-game test:** 024d server + proxy `wsock32.dll` + original client. Checklist: connect, walk, drive, shoot, call another player on the phone, die, round restarts. Run with all hooks on and then with only the new ones.
- **Network:** record traffic (Wireshark) from the original server and from the hooked server in the same scenario. Compare the packet format.
- **Progress:** `tools/progress.py` on each commit.

## First delivery after approval

All of Phase 0: Ghidra + symbols propagated to the 024d, `functions.csv`, protocol version confirmation, analysis of the client's `wsock32.dll`, and `docs/setup.md` with what to install.
