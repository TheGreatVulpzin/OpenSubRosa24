# Curiosities

Notes for a video series about how Sub Rosa works inside. Each entry says where it comes from:

- **code**: read in the decompiled code of the 024d server. The behavior is certain, the purpose is a guess.
- **game**: seen in the game.
- **check**: not confirmed yet. Check it in the game before you use it.

Do not show the game assets, the `.exe`, `config.txt` or the recordings in a video.

## Episode ideas

### 1. How the world is built

| Fact | Source | Where |
| --- | --- | --- |
| The server builds the whole level at the start of a round in a fixed order: `createlandscape`, `buildsomeshit`, `createarea`, the city intersections and streets, the blocks, then the buildings and their floor plans. | code | `createlevel` (0x54e40) |
| `buildsomeshit` is the real name of the function (from the symbols of the Linux binary). It builds track geometry with angles of pi/8 (curves of 16 segments) and calls `createtrack`. | code | 0x32fe0 |
| `buildsomeshit` is the only function that the lifted C cannot copy bit for bit. The x87 keeps 64-bit intermediate values, and the error grows through the computation. | code | `docs/lift.md` |
| The terrain is a height map of 2049 x 2049 points. | code | `createlandscape` |
| Building floor plans come from block files (`data/block/*.sbl`) that the server reads 4 bytes at a time. Loading a level makes about 3.2 million reads of 4 bytes. | code | `loadblock` (0x29a0) |
| What the track is for (a train?). | check | |
| The 4 bullet types hold real cartridge data in kilograms and meters: 7.62x39 for the AK-47, 5.56x45 for the M-16, 9x19 for the MP5, Uzi and 9mm, and .44 Magnum. Each type has the mass, the caliber, the case length and a drag factor (the cross section over the mass). | code | `setupbullettypes` (0x4a10), `src/game/bullets.c` |
| The recoil of a gun uses the momentum of the bullet: bullet mass times bullet speed, over the mass of the gun plus the bullet. | code | `itemweaponsimulation` |
| The server has elevator code. When a player uses a block of one special type inside a building, the server creates an elevator for that building and sends it to the floor of the player (height minus the building base, divided by 4). Normal city buildings seem to have no such block, so the code never runs in practice. | code, check | `city_createelevator` (0x154b0), called by `human_itemsimulation` (0x4bd60) |

### 2. The server

| Fact | Source | Where |
| --- | --- | --- |
| The dedicated server keeps about 636 MB of fixed arrays for players, objects and items. The operating system gives memory only to the pages that the game writes, so most of it costs nothing. | code | `.data` section |
| The server asks the Cryptic Sea master server for the list with a plain HTTP request to `www.crypticsea.com/anewzero/serverinfo.php`. | code | `getmasterserverip` (0x1ac40) |
| Before it reads `config.txt`, the server sets defaults: the name "Agent" plus a random number up to 511, the server name "Sub Rosa Server", and a server password that is a random number seeded with the clock. | code | `loadsaveoptions` (0x4e7a0) |
| The config reader skips everything up to the first digit or minus sign. When the value has no number, the result is whatever was on the stack. | code | `getint` (0xd110) |
| The server has a function that converts the Windows clock to the Unix clock, but the offset is 10 times too small: it subtracts it before the divide by 10. The seconds that it returns are about 330 years too big. Nothing calls it. | code | `gettimeofday` (0xb230) |
| The text reader keeps the whole file in a 1 MB buffer and never checks the size. A config or readme file over 1 MB writes over the data that follows the buffer. | code | `loadtextfile` (0xd020) |
| A list of file names has room for 1024 names of 64 bytes. A full list writes its end marker into the count of the next list, and a long name runs into the next name. | code | `listfiles` (0xd3a0) |
| The server has code that binds the process to the first CPU, but nothing calls it. The `setcpuaffinity=` key in `config.txt` is read and then ignored. | code | `setcpuaffinity` (0xb300) |

### 3. Players and chat

| Fact | Source | Where |
| --- | --- | --- |
| A chat filter looks for one racial slur. When it finds the word, it keeps the original message and replaces it with "i'm an idiot". The check is an exact match of 6 lowercase letters, so a dot or a space between the letters gets past it. | code | `server_playermessage` (0x49940) |
| The in-game newspaper reads `readme.txt` line by line and then adds a "Top 10 list" of the saved accounts, sorted by a number with a "$" (probably the money). | code, check | `newspaper_load` (0x185a0) |

### 4. Economy

| Fact | Source | Where |
| --- | --- | --- |
| The game has a stock market: functions to buy and sell stock, and a tax when a player dies. | code (names) | `playerbuystock`, `playersellstock`, `playerdeathtax` |
| How the prices move. | check | |
| `economysimulation` is dead code in the 024d server: no function calls it and no pointer to it exists in the binary. Maybe stock prices never change on a dedicated server. | code, check | `economysimulation` (0x53a0) |
| The team base has a car market (menu 6) that can never sell a car on the 024d: the list of offers (count at `0x17579740`) is never filled, so it is 0 in every snapshot (World and Round). The purchase code and its network event (`updatecarmarket`) exist, but no player can reach them. The shops' car dealer is a different system and works. | code, snapshots (2026-10-15) | `logic_playeractions` menu 6, `createevent_updatecarmarket` (0x68d0) |
| The world day lasts 3,888,000 clock units: the clock adds 10 per tick, so a day is about 104 minutes. The shops restock every 1800 ticks (about 30 seconds). | code | `logicsimulation_world` (0x5b9b0) |
| The second manager phone uses the angle at team index 1024. The item scan leaves this index behind. The angle comes from event memory. | code | `logic_setteammanager`, angle at RVA `0x953b5c8` |

### 5. Physics

| Fact | Source | Where |
| --- | --- | --- |
| The physics of humans and objects uses bonds between points (`bondsimulation2`, `humansimulation`). | code (names) | |
| One function of the 2015 server uses an SSE4.1 instruction (`dpps`, a dot product). | code | `bondsimulation2` |
| Everything that moves is made of particles (points with a mass) joined by bonds (links). A person, a car, a door and the train are all built this way. | code | `bondsimulation2` (0x41de0), `createbond` |
| A contact with the ground or a wall is just one more bond, made for that moment (state 3). The solver needs no special case for collisions. | code | `objectsimulation` (0x56710) creates `g_bond` entries from `lineintersectobject` hits |
| `bondsimulation2` is the largest function of the server (20 KB of machine code). It fixes the length of every bond, up to 16384, each tick. | code | 0x41de0 |
| The pools are fixed: 16384 bonds, 4096 particles, 1024 objects, all in static memory. The game never has to allocate memory during a match. | code | `docs/phase4-inventory.md` 4.1 |
| Vehicle collision tests every pair of objects: 1024 x 1024, about a million checks each tick. It is fast only because most objects are inactive. | code | `objectsimulation`, object pair loop |
| The same soft physics idea (points and links) is in Gish, an earlier game of Cryptic Sea. | check | compare with the Gish behavior before you use it |

### 6. Making of

| Fact | Source | Where |
| --- | --- | --- |
| The decomp runs inside the original server: a fake `wsock32.dll` that the `.exe` loads by itself replaces the functions one by one while the server runs. | project | `instrumentation/hook/` |
| Each replaced function runs twice (the C version and the original), and the verifier compares the memory that both wrote. | project | `instrumentation/hook/verify.c` |
| A float that was read as an int turned 894.0 into 1.3 billion in a building floor plan. A hardware breakpoint found the instruction. | project | `docs/lift.md`, "Watchpoint" |
| The first standalone server (no original `.exe`) ran at 9 ticks/s in place of 62.5. Every function gave the same result as the original. | code | `docs/plano.md` Phase 5 |
| The cause: the decompiler typed two loop counters (the object indexes i and j) as `float`, because the same stack slot holds a float in another part of the function. | code | `objectsimulation`, `analysis/024d/locals.csv` |
| A small integer such as 5, read as a float, is a "denormal" number. On the x87 math unit, a load of a denormal takes about 100 cycles in place of 1. In a loop of a million passes each tick, that is the whole frame. | code | `instrumentation/ghidra/FixIntLocals.java` |
| A sampling profiler (read the instruction pointer every millisecond) found it: 75% of the time was inside two physics functions, not in the main loop. | code | `instrumentation/lift/samples.py` |
| A second bug hid behind the first: the new server loaded our own test `wsock32.dll` (it sat next to the `.exe`) in place of the Windows one. | code | `src/standalone/main.c`, `load_dll` |

### 7. The client

| Fact | Source | Where |
| --- | --- | --- |
| The PSD reader consumes the plane at the declared channel count for raw data. Compressed planes stop before that index. | code | `src/client/image/psd.c`, `docs/client/psd.md` |
| The server (0.24d) and the client (0.24b) share the same game code, but they are not the same build. Of 250 functions with one name in both, only 6 have the same bytes. | code | `tools/re/compare_shared.py`, `analysis/client_win/shared_code.csv` |
| The server has a function that makes a random direction from noise (a hash with the numbers 15731 and 789221). The client has the same hash, but nothing calls it: dead code. | code | server `getrandomvector2d`, client 0x5c5240 |
| The noise hash of the texture generators uses three primes (15731, 789221 and 1376312589) that come from a well known noise article by Hugo Elias. One step is not the same: the article multiplies `n` by `n * n * 15731 + 789221`, and the game multiplies that value by itself. Checked only against copies of the article, not its first page. | code | `src/client/textures/noise.c`, `hash_unit`; [a copy of the code](https://gist.github.com/dragon0/f70e2637e6d4e64a6ab210faf8a85a50) |
| The server calls the math routines of the C library in the "intrinsic" form (the number goes in the x87 register). The client calls the plain form (the number goes on the stack). The two give the same bits for the same input, but the code is not the same. | code | `src/platform/x87_math.c`, `instrumentation/client/runtime/x87_math_client.c` |
| The server and the client keep their data in different places. A script found where, by lining up the two versions of 236 shared functions and pairing the globals at the same spot. It put `human`, `player`, `game` and `item` at the same addresses that an older, independent method gave. | code | `tools/re/pair_client_globals.py` |
| The recordings of the client reach few of the game rules (physics, humans, collision). Hosting a game seems to start a separate server process, so the client itself does not run them. | check | to confirm with a recording of a hosted game |
| The wrong money of every company in the stocks list was always $536870912. That is 0x20000000, the address where the standalone client loads its copy of the game. The lifter had lost the money argument of the text function, so it printed what lay on the stack. | code | `ApplyCallOverrides.java`, `docs/client.md` |
| The client lag that a tester saw was not a bug of the client. The PC was busy with builds and tests at the same time. A friend ran the same files with no lag. | project | `docs/client.md`, "Performance" |
| 181 of the OpenGL loader functions of the client are 10 bytes long and do nothing but return 2. They have no name inside. We named them by their order, using the Linux client as a list. | code | `tools/re/name_glee_order.py` |
| The client has its own timing code inside the game: the area builder stores the CPU cycle counter (RDTSC) in a table. The test harness had to ignore that table, or it saw a false difference. | code | `docs/client.md`, "Harness for the client" |
| A button of the main menu opens a web page. The C code calls `ShellExecuteA` for it, so the standalone client needed a small hook in the platform layer. | code | `launchwebpage`, `sr_open_url` |
| The decompiler can lose the extra arguments of a call such as `sprintf`, because it cannot see how many the format string needs. 386 calls needed a fix. A script now reads each format string and counts the arguments. | code | `instrumentation/client/check_variadic.py` |
