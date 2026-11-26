# Documents

Start here. Each row of the table names a system of the game, the folder with its code and the page that explains it.

## Where the code is

| Folder | Content |
| --- | --- |
| `src/game/` | The game rules and the simulation: humans, objects, items, physics, buildings, the level. The server and the client share it. |
| `src/common/` | Vectors, intersections, random numbers, files and the text parser. Shared. |
| `src/net/` | The bit stream, the events, the packets, the chat commands and the master server. Shared. |
| `src/server/` | The parts that only the server has: accounts, players, the main loop and its data. |
| `src/client/` | The parts that only the client has: rendering, input, sound, menus, editors, textures and image decoders. |
| `src/include/` | The types (`sr_types.h`), the data of each program, and `sr_variant.h`, the one place where the client and the server run different code. |
| `src/platform/` | The calls to the operating system and the math of the x87 unit. |
| `src/standalone/` | The start of each program, the crash report and the smoke test driver. |
| `src/lifted/` | C that a tool made from the executable. Verification only. The normal programs do not use it. |
| `instrumentation/` | The tools that compare the code with the original: the harness, the lifter, the recorder. |
| `analysis/` | The names of the functions, fields and data. The source of `sr_types.h` and the client storage. |
| `tools/` | Build scripts, generators and the launcher. |

## Systems

| System | Folder | Page |
| --- | --- | --- |
| Vectors, orientations, random numbers | `src/common/`, `src/game/mesh.c` | [game/math.md](game/math.md), [client/math.md](client/math.md) |
| Collision and intersection | `src/common/intersect.c`, `src/game/collide*.c` | [game/collision.md](game/collision.md), [client/collision.md](client/collision.md) |
| Humans (bones, walking, arms) | `src/game/human*.c` | [game/humans.md](game/humans.md) |
| Objects and vehicles | `src/game/object*.c`, `vehicles.c`, `wheels.c`, `ai_car.c` | [game/objects.md](game/objects.md) |
| Items, guns, bullets | `src/game/item*.c`, `bullet*.c` | [game/items.md](game/items.md) |
| Particles, bonds, the solver | `src/game/particles.c`, `bonds.c`, `solver.c` | [game/physics.md](game/physics.md), [client/physics.md](client/physics.md) |
| Block areas and buildings | `src/game/area.c`, `buildings.c`, `city*.c` | [game/area.md](game/area.md), [game/buildings.md](game/buildings.md) |
| Level construction | `src/game/level.c` | [game/level.md](game/level.md) |
| Players, economy, rounds | `src/game/logic*.c`, `economy.c`, `missions.c` | [game/logic.md](game/logic.md) |
| Accounts | `src/server/accounts.c`, `src/game/account_*.c` | [game/accounts.md](game/accounts.md) |
| Network protocol | `src/net/` | [protocol.md](protocol.md), [game/network.md](game/network.md), [client/network.md](client/network.md) |
| Main loop and system | `src/game/system.c`, `src/server/main_loop.c`, `src/common/system.c` | [game/system.md](game/system.md) |
| The tick and the events of the client | `src/client/setup/` | [client/simulation.md](client/simulation.md), [client/events.md](client/events.md) |
| Start of the client | `src/client/setup/`, `src/standalone/client_native_main.c` | [client/start.md](client/start.md) |
| Rendering | `src/client/render/` | [client/render.md](client/render.md), [client/landscape.md](client/landscape.md) |
| OpenGL and shaders | `src/client/gl/`, `render/glext.c`, `render/programs.c` | [client/gl.md](client/gl.md), [glee.md](glee.md) |
| Textures, noise, models | `src/client/textures/` | [client/textures.md](client/textures.md), [client/models.md](client/models.md) |
| Image formats | `src/client/image/` | [client/load.md](client/load.md), [client/png.md](client/png.md), [client/jpeg.md](client/jpeg.md), [client/bmp.md](client/bmp.md), [client/tga.md](client/tga.md), [client/hdr.md](client/hdr.md), [client/psd.md](client/psd.md), [client/zlib.md](client/zlib.md), [client/io.md](client/io.md) |
| Bitmap fonts | `src/client/image/fonts.c`, `render/text.c` | [client/fonts.md](client/fonts.md) |
| Input (keyboard, mouse, controls) | `src/client/input/` | [client/input.md](client/input.md) |
| Sound and voice | `src/client/audio/` | [client/audio.md](client/audio.md) |
| Menus | `src/client/menu/` | [client/records.md](client/records.md) |
| Editors | `src/client/editor/` | [client/editor.md](client/editor.md) |
| Debug and timers | `src/client/debug/` | [client/runtime.md](client/runtime.md) |
| Code shared by the client and the server | `src/include/sr_variant.h` | [client/shared.md](client/shared.md) |
| Smoke test of the client | `src/standalone/client_smoke*.c` | [client/smoke.md](client/smoke.md) |

## Formats, versions and tools

| Topic | Page |
| --- | --- |
| File formats of the game (`.srk`, `.sbl`, `.sbb`, `.sbc`, `.csm`) | [formatos.md](formatos.md) |
| Records and types | [structs.md](structs.md) |
| The versions of the game | [versoes.md](versoes.md), [versoes-todas.md](versoes-todas.md) |
| The lifter and the harness | [lift.md](lift.md), [side-effects.md](side-effects.md), [hooks.md](hooks.md) |
| How the client was rewritten | [client_rewrite.md](client_rewrite.md), [client_final_plan.md](client_final_plan.md) |
| The server migration to C | [server-c-migration.md](server-c-migration.md) |
| Setup of a work station | [setup.md](setup.md) |
| Releases | [release.md](release.md) |
| Plan and progress | [plano.md](plano.md), [progresso.md](progresso.md) |
| Ideas and curiosities | [ideas.md](ideas.md), [curiosities.md](curiosities.md) |
