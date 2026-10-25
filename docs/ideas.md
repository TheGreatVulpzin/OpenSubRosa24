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

These checks cover invalid client input and local data files. Valid inputs keep the same behavior. Network details: `docs/protocol.md`, section 12, item 15.

| Fix | Where | State |
| --- | --- | --- |
| A1: mask the voice frame index to six bits | `handle_input`, `src/net/server.c` | Done in the server, including standalone. The proxy guard stays unchanged. |
| A2: mask the action start index before the first action write | `handle_input`, `src/net/server.c` | Done. The action ring has 64 entries. |
| A3: mask the action start index with zero actions | `handle_input`, `src/net/server.c` | Done. Prevents an endless loop in `logic_playeractions`. |
| A4: ignore input from a connection with a negative `player_id` | `handle_input`, `src/net/server.c` | Done. Covers the old connection after an account takeover. |
| A5: cap the connection pool at 256 | `handle_join`, `add_connection`, `src/net/server.c` | Done. At capacity, a new account cannot join. A connected account reuses its current slot. |
| A6: discard short joins and inputs before state changes | `serverrecv`, `src/net/packet.c` | Done. Includes input trailers. A short read stops all later reads until the next packet. |
| B1: check the phone item index before each write | `give_phone`, `src/net/commands.c` | Done. A full item pool cannot cause a write outside the pool. |
| B2: ignore `/phone` when the admin has no human | `give_phone`, `src/net/commands.c` | Done. The command returns before it removes phones or reads human data. |
| B3: stop the kick list at 41368 entries | `kick_player`, `src/net/commands.c` | Done. The last free entry works. A full list ignores the command. |
| B4: bound the `/message` copy with `strnlen` and add a null byte | `admin_commands`, `src/net/commands.c` | Done. The copy reads at most the 55 bytes left in the chat buffer. |
| B6: convert each chat byte to `unsigned char` before `tolower` | `server_playermessage`, `src/net/commands.c` | Done. Bytes 128 through 255 stay within the input range of `tolower`. |
| C1: check both bounds of menu 6 before the table access | `logic_playeractions`, `src/game/logic.c` | Done. Ignore values below 1 or with an index at or above `car_base`. The value 16 still closes the menu. |
| C2: check item creation in the dealer, wallet, bank, and car market | `logic_cardealer_buy`, `item_action`, `bank_action`, `logic_playeractions`, `src/game/logic.c` | Done. If the item pool is full, cash transfers keep their balances and bills. Car purchases still complete but skip the key write and link when no item slot is free. |
| D1: reserve space for the ranking in `readme.txt` | `newspaper_load` | Done. File text stops at 1013 lines. The title and ten accounts fit in 1024 lines. |
| D2, D4: bound material and block counts in `.sbb` files | `building_load` | Done. Reject negative counts and counts above 256 before changes to the name tables. |
| D3: bound each building dimension | `building_load` | Done. Accept 0 through 63, including header reads. Invalid dimensions use the missing-file path. |
| D5: bound building and block paths | `building_load`, `building_save`, `loadblock` | Done. Use `snprintf`. Reject `/`, `\`, `..`, and names that do not fit. |
| D6: bound surface and box counts in `.sbl` files | `loadblock` | Done. Accept 0 through 1024. Invalid counts and short reads keep the current block. |
| D7: bound city counts | `leveleditor_load`, `src/game/block_mesh.c` | Done. Accept at most 1024 intersections, 512 streets, and 1024 buildings. Invalid counts and short reads keep the current city. |
| D8: reserve the last parser byte | `loadtextfile` | Done. Read at most the buffer size minus one. Oversized files give no text. |
| D9: bound file lists and name copies | `listfiles` | Done. Use `strncpy`. Lists hold at most 1023 names. Each name ends with a zero byte. |
| D10: check account files | `sr_loadaccountsserver` | Done. Accept 0 through 65535 accounts. Check each read. Set `name[31]` to zero. Invalid files keep the new key and give no accounts. |

S4 tests: `tools/formats/test_s4.py` covers bounds, short reads, path names, memory sentinels, and all supported data files.
Run it with a recording directory and a work directory in `build/`.
Use `--data` for the game data directory and `--image` for the game image of the harness.
The building, level, area, and system tests also check valid inputs.

## Security work reserved for SVS

SubVulpes Server (SVS) inherits the completed OpenSubRosa checks above.
The following items remain open in SVS because they change behavior for valid inputs or add server policy.
They are not part of the completed S1 through S4 checks.

| Item | Work reserved for SVS |
| --- | --- |
| A7 | Clamp float inputs. Define the accepted range for each field. |
| A8 | Limit the rate of query replies. |
| A9 | Define a window for action replay. |
| A10 | Deferred. The security plan lists this identifier without a description. Define its scope before implementation. |
| B5 | Count admin login attempts by IP address and require a strong default password. |
| C3 | Handle object item lists above the 255 entries that the wire count can represent. |
| C4 | Handle human health above 117 without a wrap in the 7-bit field. |
| D11, D12 | Deferred. The security plan lists these identifiers without descriptions. Define their scope before implementation. |
| Persistent `ban_time` | Save and load ban time. The account file does not store this field. |
| Server name | Limit the server name to 31 characters and a null byte. The query reply sends 32 bytes. |

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
- S2 and S3 checks are complete. See B1 through B4, B6, C1, and C2 in the security table above.
- The server checks also apply to the standalone server. `hook/netguard.c` stays unchanged.


## System code (found in group 7)

- Call `timeBeginPeriod(1)` before the main loop. The loop sleeps 1 ms, and the system timer resolution sets how long the sleep lasts, so the ticks have jitter. Measure the tick time with and without it. (SubVulpes)
- The S4 checks bound file sizes in `loadtextfile` and name copies in `listfiles`. See D8 and D9 above. (both)
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
