# SubVulpes plan — Sub Rosa decomp

## Context

The repository is already initialized (commit `b8ca658`): `original/` outside git with a SHA-256 manifest, the friend's server manager in `third_party/server-manager`, tools in `tools/re/`, inventory in `docs/inventario.md`.

Goal: decompile Sub Rosa to improve servers, add features, optimize and port (Linux, macOS, browser).

The work has two parts, in this order:

1. **The decomp** (Phases 0 to 7): readable C that does the same as the original game and server. The harness proves each function against the original.
2. **Two products on top of the decomp** (after Phase 7). Names are temporary:
   - **OpenSubRosa:** the original game and server with optimizations, light QOL and more platforms. It stays compatible with the original 0.24 clients and servers.
   - **SubVulpes:** a server with a plugin manager and stronger admin tools, and a client that can have features the original game does not support. Where possible it works with the original clients too, and gives extra features to SubVulpes clients.

Until the decomp is done, the two products are only ideas in `docs/ideas.md`. Code changes that make the game different from the original wait for the end of Phase 7.

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

## Phase 4 — Names (closed 2026-10-17)

Make the lifted C readable without hand work. All names go into the Ghidra project with scripts and into the repository (`analysis/*.csv`, `sr_types.h`), never into `lift.py` (see `CLAUDE.md`).

| Measure in the lifted C | Start | End |
| --- | ---: | ---: |
| `DAT_` names | 14870 | about 5000 |
| Different unnamed addresses | 1473 | 437 |
| Inline `SR_VA` | 657 | 239 |
| Typed `unk_` fields used | 1400 | more than 4400 |

What was done:

1. Globals: about 50 globals with a name, a size and a type (`analysis/024d/data_globals.csv`, `manual_globals.csv`, `tools/re/gen_types.py`). The evidence: equal bytes in the initialized `.data`, or a Linux 0.24c size that fits the gap to the next known global (`tools/re/map_regions.py`).
2. Field types: `tools/ghidra/InferFields.java` and `tools/re/infer_fields.py` read the type of each field from the instructions (`analysis/structs/024d/`).
3. Addresses inside a typed global print by name: `((int)&g_player[3].ai_waypoint)`, `((int)&g_human[512])` (`LiftExport.java`, `namedAddress`).
4. Five pointer parameters with their pointer type (`params.csv`).

Left on purpose:

- About 200 addresses: small statics, the MSVC CRT data, and globals that only the client uses (`texturelist`, `modellist`). The client globals come back in Phase 7.
- Local variable names (`iVar2`, `local_4`): Phase 6 rewrites the functions by hand.
- One file for each module: skipped. Five tools read `lifted_024d.c`. In Phase 6 each rewritten function moves to its module (`src/net/`, `src/game/`), so the lifted file gets smaller by itself.

## Phase 5 — Standalone server

A server executable that runs without `subrosadedicated024d.exe`. It runs the same C as the hooked build, so it behaves the same as the original.

Phase 3 is closed (2026-10-16, `docs/progresso.md`): 321 of 340 functions are in C (99.2 % of the code bytes). The lifted C reaches the image only through `SR_VA(va)` = `sr_base + va - 0x400000` (`src/lifted/lift_support.h`) and `sr_import`. Counts in `src/lifted/lifted_024d.c` on 2026-10-16:

| Use of the image | Count | Target |
| --- | ---: | --- |
| `SR_VA` into `.data` (globals, `#define` and inline) | 4968 | 639 of them inline, without a name |
| `SR_VA` into `.text` | 626 | 625 calls to other functions in C, through the hook `JMP` in the `.exe`. 1 is `LAB_0040d320`, the `qsort` comparator, which is not in C. |
| `SR_VA` into `.rdata` | 27 | 18 inline. Strings are already C literals, and float constants are already exact C values (`exactFloat`). |
| `sr_import` | 123 references | Reads the import table of the PE image at `sr_base`. |

Relocations outside `.text`: 15 in the initial `.data` (all point to `.rdata` strings) and 13 in `.rdata` (CRT tables). The initial `.data` has no pointer to `.text`.

1. **5.1: image region and direct calls** (2 to 3 days).
   - The new executable reserves a region of `SizeOfImage` (`0x25f3b000`, about 607 MB, the same as the original) and sets `sr_base` to it.
   - A script generates C arrays of `.rdata` and the initial `.data` (versioned in `src/`). At start-up the server copies them to their RVAs in the region and applies the 28 relocations.
   - `lift.py` generates direct calls to the `lf_` functions for the standalone build. The hooked build keeps the calls through the `.exe`, so that the live hooks and the verifier still see every call.
   - Write the `qsort` comparator (`0xd320`) in C.
2. **5.2: platform layer** (1 to 2 days). Replace `sr_import` with direct links to the CRT and WinSock. Sockets, timer and files go to `src/platform/`. Keep the x87 precision at 53 bits.
   - Done 2026-10-17: `src/platform/plat_imports.c` links every import of the game at build time. The server does not load `MSVCR100.dll`, and its speed did not change (61 to 62 ticks/s).
   - `src/tests/crt_diff.c` compared the two CRTs first: the x87 math intrinsics, `sscanf`, `rand`, `qsort`, `tolower` and `floor` give the same bits. `sprintf` differs only for `%e` and `%f`, and the game prints only `%d`, `%s` and `%03d`.
   - WinSock: most `WSOCK32.dll` functions go on to `ws2_32.dll`, so they link to `ws2_32`. `recv`, `recvfrom` and `getsockopt` have their own code in `WSOCK32.dll` and come from it (System32 only).
3. **In-game test** with players. The 39 harness-only functions (`serverrecv`, `serversend`, `servercycle`...) run live in C here for the first time. Look at them first when a network bug shows.

The harness keeps testing the functions against the 024d. No packet replay test for now. Add it if a bug is too hard to find in game.

## Phase 6 — Readable server

Rewrite every server function by hand into clean C (`src/common/`, `src/net/`, `src/game/`, `src/server/`), as `src/server/accounts.c` and `events.c` already do. The lifted version stays as the fallback until the rewrite is done.

- A rewritten function has its real name (`analysis/024d/functions.csv`), real types and real variable names. `lift.py` finds it by that name and lifts it no more. The table of the harness and the hooks use the same name. The code does not depend on the transition tools: when the rewrite ends, the game compiles without `lift.py`.
- The x87 math of the game CRT is `x87_sqrt`, `x87_sin` and the others (`src/include/sr_math.h`).
- First file: `src/game/vectors.c` (13 vector functions, 2026-10-17).
- Folders: `src/common/` (math, shared by the client and the server), `src/net/` (packets and events), `src/game/`, `src/server/`. The operating system calls of the clean code go through `src/platform/os_calls.c` (`sr_os.h`), so the harness and the recording build see them.
- Done: math (`src/common/vectors.c`, `random.c`), packets (`src/net/packet.c`) and events (`src/net/events.c`). Particles and bonds (`src/game/particles.c`), the bond pair functions (`src/game/solver.c`) and the bond solver `bondsimulation2` (`src/game/bonds.c`, 2026-10-18: bench.py 1.02 against the original). `bondsimulation2` still calls five functions in the lifted C: `stoptimer`, `multiply3x3matrices`, `objectwheelsimulation2`, `calculatefriction` and `objectenginesimulation2`.
- Done on 2026-10-19 (after lot C4): `src/lifted/sr_types_lifted.h` is the frozen header of the lifted C, `gen_types.py` checks the sizes against it, and `pipeline.sh` runs Ghidra only with `--export`. The reason and the plan:
- Stop the daily use of Ghidra. Each naming pass costs about 15 minutes of ApplyTypes and LiftExport, and a name that leaves `sr_types.h` breaks the lifted C (as `unk_0090` in `objects.c`). The plan:
  1. Freeze `src/lifted/lifted_024d.c` with a header of its own: a copy of today's `sr_types.h`, with the old names (`unk_` and the others).
  2. The clean code (`src/common`, `src/net`, `src/game`, `src/server`) keeps the `sr_types.h` from `gen_types.py`. New names go only there.
  3. The `SR_STATIC_ASSERT` checks of size and offset in both headers make sure that the layout is the same.
  4. `lift.py` stays, only to make `lifted_table.c` and `hooks_lifted.c` and to take each rewritten function out of the lifted C. It reads the export that exists, without Ghidra.

  Then a new name takes seconds. The lifted C keeps the old names, but it gets smaller with each rewritten function. Cost: 1 to 2 hours, once. Check it with the build, the full gate and the synthetic tests (`synth_bonds.py`, `synth_humans.py`).
- Not tested by a recording: the `bondsimulation2` bond types 3 (a particle against a triangle of an object), 11 (an item that turns with the hand), 13 (bone against bone of two humans), 14 (bone against an item) and 15 (a wheel body on the ground). `tools/lift/synth_bonds.py` adds bonds of these types to a recording snapshot. The harness gives the same result for each type (2026-10-18). A broken copy of each helper gives a difference, so the test reaches them. The values are made up, so a Harness Recording with fights, held guns, loose items, car crashes and a train is still useful. No snapshot has an object with wheel bodies: type 15 uses the first record of object 0.
- Waiting: `packevent` and `packobjectpacket` (the recordings run 21% and 47% of their blocks: they wait for a Harness Recording test), `createevent_updatestocks` and `createevent_updatecarmarket` (with the economy, when `Market` has field names), `eventsimulation` (with the doors of the buildings).

- A rewritten function replaces the lifted one only when the harness shows 0 differences on all recordings.
- A rewritten function must not be slower than the original. Readable code is not a reason for slow code. Measure both versions with the per-hook profiler (`subvulpes.ini profile=1`) on the same server load. Example of what to avoid: `src/server/main_loop.c` works, but it is not well optimized. Rewrite it with the same care as the other functions.
- An optimization that keeps the harness at 0 differences is allowed in this phase. It is not a change of behavior.
- No change of behavior in this phase. When a function shows a place for a future change ("an admin command could go here"), write it in `docs/ideas.md`, not in the code.
- Order: the groups below, in this order. The level generation comes last.

### The groups of the rewrite (2026-10-19)

Done: math (`vectors.c`, `random.c`, `intersect.c`), packets and events (`packet.c`, `events.c`), accounts (`accounts.c`), items and bullets in part (`items.c`, `bullets.c`), particles and bonds (`particles.c`, `solver.c`, `bonds.c`), `humansimulation` (`humans.c`), and the collision tests against the level, the landscape, the objects, the rails and the city objects (`collide.c`, `rigid.c`). Group 1 (2026-10-19): the block areas and the 2D tests (`area.c`, `collide_area.c`). Group 2 (2026-10-19): the rest of the humans (`human_body.c`, `human_pose.c`, `human_arms.c`). That is 132 of 316 functions.

Next, in this order (sizes from `analysis/024d/functions.csv`):

| # | Group | Functions | Size |
| --- | --- | ---: | ---: |
| 1 | Collision with the block areas, and the 2D tests (done) | 29 | 22 KB |
| 2 | Humans, the rest (done) | 11 | 18 KB |
| 3 | Items and weapons (the rest) | 7 | 16 KB |
| 4 | Objects and vehicles | 27 | 67 KB |
| 5 | Server network | 24 | 36 KB |
| 6 | Players, logic, economy and missions | 47 | 42 KB |
| 7 | System and main loop (also a rewrite of `src/server/main_loop.c`) | 22 | 5 KB |
| 8 | Map and buildings | 57 | 72 KB |

1. Collision with the block areas, and the 2D tests: `sphereintersectarea`, `lineintersectarea`, `circleintersectarea`, `sphereintersectnormalblock`, `sphereintersectspecblock`, `lineintersectnormalblock`, `lineintersectspecblock`, `circleintersectnormalblock`, `circleintersectspecblock`, `lineintersectcityobjecttype`, `lineintersecttrack`, `lineintersecthuman`, `item_sphereintersect`, `sphereintersectpoint`, `sphereintersectline2d`, `lineintersectline2d`, `lineintersectcircle2d`, `pointdistanceline`, `pointinsidebase`, `pointinsidebasesafe`, `pointinsidebuilding`, `rotatevector2d`, `area_getblock`, `area_getblocktexture`, `area_interior_getblock`, `area_interior_getblocktexture`, `area_interior_getblocklink`, `area_getnextchunk`, `area_getnextsector`.
2. Humans: `human_calculatearmangles`, `human_calculatelegangles`, `human_calcarmtorque`, `human_collisionhuman`, `createhuman`, `deletehuman`, `human_buildmuscles`, `human_createmuscleball`, `createjointbond`, `setupbody`, `setitempositionhuman`.
3. Items and weapons: `itemsimulation`, `human_itemsimulation`, `item_physicssimulation`, `itemlogic`, `itemweaponsimulation`, `bulletsimulation`, `setupitemtypes`.
4. Objects and vehicles (also the lifted-style files `objects.c`, `wheels.c`, `ai_car.c`): `objectsimulation`, `objectsimulation_car`, `objectsimulation_heli`, `objecttrainsimulation`, `object_rotorsimulation`, `objectwheelsimulation2`, `objectenginesimulation2`, `objectwheelcollision`, `objectwheelcollision2`, `getenginetorque2`, `getobjectrotation3`, `addobjectrotation`, `applydamageobject`, `objecttimetolive`, `createobject`, `createvehicle`, `objecttype_addbond`, `objecttype_addface`, `objecttype_addparticle`, `objecttype_addwheel`, `objecttype_calcquadweights`, `setupobjecttypeweight`, `setupobjecttypes_cars`, `setupobjecttypes_helicopters`, `setupobjectypes_train`, `ai_playercar`, `ai_car_nextwaypoint`.
5. Server network: `serverrecv`, `serversend`, `servercycle`, `packevent`, `unpackevent`, `packobjectpacket`, `packobjectpacket2`, `unpackobjectpacket`, `packpositiondelta`, `unpackpositiondelta`, `packscalar`, `packunitvector`, `unpackangle`, `unpackorientation`, `pbitreadnego`, `clientrecv`, `getmasterserverip`, `disablesocket`, `server_playermessage`, `calculateplayervoice`, `addvoice`, `createevent_updatecarmarket`, `createevent_updatestocks`, `eventsimulation`.
6. Players, logic, economy and missions: `createplayer`, `deleteplayer`, `playersimulation`, `playerai`, `player_calcassets`, `player_loadinventory`, `player_saveinventory`, `playerbuystock`, `playersellstock`, `playerdeathtax`, `playerteamkilltax`, `createaccount`, `account_nameexists`, `loadaccountsclient`, `saveaccountsclient`, `saveaccountsserver`, the 10 `logic_*`, the 5 `logicsimulation*`, the 3 `economy*`, the 3 `createmission_*`, `createscenario_world`, the 7 `scenario_*`, `resetgame`, `setupgame`, `ai_assessthreats`.
7. System and main loop: `setcpuaffinity`, `getcpufrequency`, `gettimeofday`, `systemtimer`, `testtimer`, `setuptimers`, `server_writelogfile`, `debug_resetlogfile`, `loadsaveoptions`, `optionreadwriteint`, `loadtextfile`, `listfiles`, `findstring`, `getstring`, `getfloat`, `cs_getline`, `cs_pread`, `csradixsort`, `removeflieextentions`, `sprintf_comma`, `__aulldiv`, `init_by_array`.
8. Map and buildings: `createlevel`, `createlandscape`, `landscape_buildsectorlods`, `createarea`, `createtrack`, `buildsomeshit`, `buildcitystreet`, `buildcityinterection`, `addcitystreet`, `city_addintersection`, `city_createelevator`, `createcityobject`, `cityobject_addbox`, `setupcityobjecttypes`, `setupbuildingtypes`, the 6 `building_*`, the `area_create*`, `area_block_*`, `area_interior_create*`, `area_interior_set*` and `area_set*` functions, `area_copyrotateblock`, `area_deleteblock`, `area_breakblock`, `area_marksector`, `loadblock`, `leveleditor_load`, `leveleditor_save`, `newspaper_load`, `addbeziersurface`, `addbeziersurfacetexture`, `tesselatebezier`, `tesselatebezierderivative`, `calculatepointscale`, `cerpnoise2d`, `lerpnoise2d`.

Groups 5 and 6 hold the slow lifted functions of the table below (`packobjectpacket`, `packevent`, `logicsimulation_world`, `logicsimulation`): they run each tick, some for each player.
- `tools/progress.py` counts the hand-written functions.
- Speed: `python -I tools/lift/bench.py --all` at the end of each lot (the harness times each function on the recorded calls, `HARNESS_TIME=1`). The times move by about 30% from one run to the next: only large ratios that repeat mean something.
- Slow lifted functions (bench.py, 2026-10-18, recording 20261013-195210, 3 runs). Fix each one when its lot comes. The likely cause is the old one: float locals that hold integers (denormals on the x87, `FixIntLocals.java`).

  | Function | C / original | When it runs |
  | --- | --- | --- |
  | `logicsimulation_world` | 9.5 to 9.7 | each tick |
  | `building_buildfloorplan` | 4.5 to 7.1 | level load |
  | `packobjectpacket` | 3.3 to 5.2 | each tick, for each player |
  | `logicsimulation` | 3.7 to 5.7 | each tick |
  | `packevent` | 2.6 to 3.8 | each tick, for each player |
  | `deleteplayer` | 2.4 to 3.2 | a player leaves |
  | `sphereintersectcityobjecttype` | 2.4 to 3.0 | collisions |

  The clean `humansimulation` (lot C4, 2026-10-19) gives 1.05 to 1.12 in 3 runs. The lifted version gave 1.17. The time includes its lifted callees (`lineintersectlevel`, `sphereintersectlevel`, `sphereintersectobject`), the likely cause. Measure it again after the lot of the collisions. `bondsimulation2` gives 0.97 to 1.05.

Exception: security fixes for invalid client input (`docs/protocol.md`, section 12, item 15) may go in early. The recordings have only valid input, so the harness does not change. The voice index fix is already in `hook/netguard.c`.

## Phase 7 — Client decomp

Apply the same method to `subrosa.exe` (it also imports `WSOCK32.dll`): hooks, lift, harness, names, standalone, readable rewrite. The client shares `common/`, `game/` and `net/` with the server. New parts: render, input, audio, voice, menus, editors.

## After the decomp: OpenSubRosa and SubVulpes

The ideas for both products are in `docs/ideas.md`, with the product each one belongs to. The main groups:

- **OpenSubRosa:** optimization (`servercycle`, tick rate, packets), dynamic pools with configurable limits, Linux / macOS / Windows x64, SDL2 + OpenAL + GL 3.3, browser (WASM), light QOL behind options (option off = original).
- **SubVulpes:** plugin manager, admin commands and RCON, the server manager modules as plugins, anti-cheat, own server list, client features that need a new protocol.

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
