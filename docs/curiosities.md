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
| The server has elevator code. When a player uses a block of one special type inside a building, the server creates an elevator for that building and sends it to the floor of the player (height minus the building base, divided by 4). Normal city buildings seem to have no such block, so the code never runs in practice. | code, check | `city_createelevator` (0x154b0), called by `human_itemsimulation` (0x4bd60) |

### 2. The server

| Fact | Source | Where |
| --- | --- | --- |
| The dedicated server keeps about 636 MB of fixed arrays for players, objects and items. The operating system gives memory only to the pages that the game writes, so most of it costs nothing. | code | `.data` section |
| The server asks the Cryptic Sea master server for the list with a plain HTTP request to `www.crypticsea.com/anewzero/serverinfo.php`. | code | `getmasterserverip` (0x1ac40) |
| Before it reads `config.txt`, the server sets defaults: the name "Agent" plus a random number up to 511, the server name "Sub Rosa Server", and a server password that is a random number seeded with the clock. | code | `loadsaveoptions` (0x4e7a0) |
| The config reader skips everything up to the first digit or minus sign. When the value has no number, the result is whatever was on the stack. | code | `getfloat` (0xd110) |

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

### 5. Physics

| Fact | Source | Where |
| --- | --- | --- |
| The physics of humans and objects uses bonds between points (`bondsimulation2`, `humansimulation`). | code (names) | |
| One function of the 2015 server uses an SSE4.1 instruction (`dpps`, a dot product). | code | `bondsimulation2` |

### 6. Making of

| Fact | Source | Where |
| --- | --- | --- |
| The decomp runs inside the original server: a fake `wsock32.dll` that the `.exe` loads by itself replaces the functions one by one while the server runs. | project | `hook/` |
| Each replaced function runs twice (the C version and the original), and the verifier compares the memory that both wrote. | project | `hook/verify.c` |
| A float that was read as an int turned 894.0 into 1.3 billion in a building floor plan. A hardware breakpoint found the instruction. | project | `docs/lift.md`, "Watchpoint" |
