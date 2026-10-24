# Ideas for after the decomp

Changes that make the game or the server different from the original. They wait for the end of the decomp (`docs/plano.md`, Phase 7). During the decomp, write each new idea here, not in the code.

Products (names are temporary):

- **OpenSubRosa:** the original game and server with optimizations, light QOL and more platforms. Always compatible with the original 0.24 clients and servers.
- **SubVulpes:** a server with a plugin manager and stronger admin tools, and a client with features that the original game does not support. Where possible it works with the original clients too.

Rule: an idea that breaks compatibility with the original 0.24 goes to SubVulpes. An idea that keeps it can go to either product.

Columns: **Product** is OpenSubRosa, SubVulpes or both. **Where** is the function or the file where the change would go, when it is known.

## Server

| Idea | Product | Where | Notes |
| --- | --- | --- | --- |
| Dynamic pools with configurable limits (players, objects, items) | OpenSubRosa | the global arrays of `.data` (606 MB) | Default limits = the original limits. Also necessary for WASM. |
| Profile and optimize the tick (tick rate, packet sending, collisions) | OpenSubRosa | `servercycle`, `serversend`, `bondsimulation2` | `hook/` has a per-hook profiler (`subvulpes.ini profile=1`). |
| Speed of the level build: `buildcityinterection` and `buildcitystreet` measured 1.06 to 1.07 against the original in three full runs, and `createlevel` 1.23 in two calls. They run at map load only | OpenSubRosa | `src/game/city.c`, `src/game/level.c` | Measure again with `bench.py --all-calls`. The cause is not known. |
| Linux x64 and macOS servers | OpenSubRosa | `src/platform/` | SSE in place of x87 changes the float bits. Decide the accepted precision first. |
| Own server list if the Cryptic Sea master does not answer | both | `getmasterserverip` | The master is `crypticsea.com/anewzero/serverinfo.php`. |
| Native plugin API (C, loaded from a DLL or `.so`) | SubVulpes | | Replaces the memory patches of the server manager. Plan with Lua and WASM hosts, hash check and game modes: [plugins.md](plugins.md). |
| Plugin events: chat, death, connect, disconnect, tick | SubVulpes | `serverrecv` (chat), the death code of `humansimulation`, `servercycle` | |
| Admin commands: `/heal`, `/tp`, `/npc`, `/imortal`... | SubVulpes | chat handling in `serverrecv`, `createevent_message` | Ported from `third_party/server-manager`. |
| Server manager modules as plugins: black market, death messages, faster truck, starting money, punches, bots | SubVulpes | `logic_*`, `item_*`, `humansimulation` | See `third_party/server-manager/ANALISE-*.md`. |
| Admin console / RCON, structured logs | SubVulpes | `hook/console.c` is a start | |
| Server-side anti-cheat checks | SubVulpes | `serverrecv` | |

## Security fixes (allowed during the decomp)

These change only what happens with invalid client input. The recordings have only valid input, so the harness does not change. Details: `docs/protocol.md`, section 12, item 15.

| Fix | Where | State |
| --- | --- | --- |
| A1: mask the voice frame index to six bits | `handle_input`, `src/net/server.c` | Done in the server, including standalone. The proxy guard stays unchanged. |
| A2: mask the action start index before the first action write | `handle_input`, `src/net/server.c` | Done. The action ring has 64 entries. |
| A3: mask the action start index with zero actions | `handle_input`, `src/net/server.c` | Done. Prevents an endless loop in `logic_playeractions`. |
| A4: ignore input from a connection with a negative `player_id` | `handle_input`, `src/net/server.c` | Done. Covers the old connection after an account takeover. |
| A5: cap the connection pool at 256 | `handle_join`, `add_connection`, `src/net/server.c` | Done. At capacity, a new account cannot join. A connected account reuses its current slot. |
| A6: discard short joins and inputs before state changes | `serverrecv`, `src/net/packet.c` | Done. Includes input trailers. A short read stops all later reads until the next packet. |
| Item count of the object part (8 bits, no cap) | `serverrecv` | Open |
| Server name copy (`strcpy`, longer than the 32 bytes sent) | | Open |
| Human health (`health + 10` in 7 bits, wraps above 117) | | Open |
| Line count of `readme.txt`: a full file writes past the line array (1033 lines of 256 bytes in an array of 1024) | `newspaper_load` | Open. The input is a server file, not client input. |
| Material and block counts of a `.sbb` file have no limit. `building_load` writes past the name arrays (256 of 64 bytes). `building_setupbuild` indexes `mat_map[256]` and `block_map[1024]` on the stack | `building_load`, `building_setupbuild` | Open. The input is a data file. It matters when packages can bring buildings. |

## Client

| Idea | Product | Where | Notes |
| --- | --- | --- | --- |
| Standalone client with SDL2 + OpenAL | OpenSubRosa | | |
| GL 3.3 / GLES 3 in place of the fixed-pipeline OpenGL | OpenSubRosa | render | Necessary for WebGL2. |
| Linux, macOS (arm64), Windows x64 clients | OpenSubRosa | `src/platform/` | |
| Browser (Emscripten / WASM) | both | | WebGL2 render. The browser cannot use UDP: the server needs a WebSocket or WebRTC DataChannel transport next to UDP. The player loads their own copy of the game files (file picker + IndexedDB), no hosted assets. |
| Light QOL behind options (option off = original) | OpenSubRosa | | |
| Client features that need a new protocol | SubVulpes | | The SubVulpes server keeps the original protocol for original clients. |

## Security fixes for the network code (found in group 5)

Each one changes behavior only for invalid input from a client or an admin. The list with the reason of each is in `docs/protocol.md`, section 12, item 15.

- S1 checks are complete. See A1 through A6 in the security table above.
- Test `it < 0x400` before the write in `/phone` (`src/net/commands.c`).
- Stop the kick list at 41368 entries.
- The server checks also apply to the standalone server. `hook/netguard.c` stays unchanged.


## System code (found in group 7)

- Call `timeBeginPeriod(1)` before the main loop. The loop sleeps 1 ms, and the system timer resolution sets how long the sleep lasts, so the ticks have jitter. Measure the tick time with and without it. (SubVulpes)
- Check the size of the file in `loadtextfile`. A `config.txt` or `readme.txt` over 1 MB writes past the parser buffer. (both)
- Check the name length and the list size in `listfiles`. A name of 64 bytes or more runs into the next name, and a list with 1024 names writes a 0 into the next list. (both)
- Stop `getint` from returning a random value when the text has no number. The clean code returns 0. (both)
- Fix the offset of `gettimeofday` (11644473600000000 in place of 116444736000000000). The function is not used now. (both)

## Big ideas (dreams, not plans)

All are for SubVulpes and need the plugin API first ([plugins.md](plugins.md)). All work with the original client unless a note says otherwise. Each part is useful alone.

| Idea | What it needs | Notes |
| --- | --- | --- |
| Replay and rewind | A log of the inputs, the `os_rand()` seed and the tick clock. Snapshots | The server is deterministic. Replay, kill-cam, rewind by 30 s and proof against cheats. A snapshot is about 600 MB, so it needs incremental snapshots or smaller pools. A plugin that reads outside data must record it. |
| Bots that talk | Plugin with a language model, chat and phone hooks | The original client sees only text messages. |
| Persistent world | A plugin that saves buildings, territories, stocks and reputation between rounds | The game already has economy, stocks, accounts and missions. A government plugin can charge taxes and run elections by chat. |
| Event director | Creation of items, objects and bots, and chat messages | A convoy, a police sweep or a truck strike. The difficulty follows the group. The cheapest big idea. |
| Linked servers | A safe account identity, and a server to server API | A player moves between servers with money and items. The protocol sends `account_id` as plain text, so identity needs new work. |
| Game modes as packages | Teleport, items, chat, rounds | Battle royale with a shrinking zone, capture the flag, races, a campaign with missions in a chain. |
| Discord and Twitch bridge | Chat events and a network plugin | The in-game chat and a Discord channel are one. The audience votes for events or spends points on items. |
| Live web map and analytics | A plugin that sends positions, deaths and stock prices over WebSocket | A top-down map of the city in a browser, heat maps of deaths, charts of the economy. Read only, so it is safe. |
| Seasons, ladder and achievements | Accounts and a database | Rankings by season, rewards, titles. |
| Mutators | State changes only | For example a random item for everyone each minute, or a bot swarm. Do not change physics: the client calculates it alone and would desync. |
| Random cities | The level builder of `src/game/level.c` | Only if the client builds the same level from a shared seed. If the client reads its own map files, a changed map desyncs. Check this first. |
