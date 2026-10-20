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
| Linux x64 and macOS servers | OpenSubRosa | `src/platform/` | SSE in place of x87 changes the float bits. Decide the accepted precision first. |
| Own server list if the Cryptic Sea master does not answer | both | `getmasterserverip` | The master is `crypticsea.com/anewzero/serverinfo.php`. |
| Native plugin API (C, loaded from a DLL or `.so`) | SubVulpes | | Replaces the memory patches of the server manager. |
| Plugin events: chat, death, connect, disconnect, tick | SubVulpes | `serverrecv` (chat), the death code of `humansimulation`, `servercycle` | |
| Admin commands: `/heal`, `/tp`, `/npc`, `/imortal`... | SubVulpes | chat handling in `serverrecv`, `createevent_message` | Ported from `third_party/server-manager`. |
| Server manager modules as plugins: black market, death messages, faster truck, starting money, punches, bots | SubVulpes | `logic_*`, `item_*`, `humansimulation` | See `third_party/server-manager/ANALISE-*.md`. |
| Admin console / RCON, structured logs | SubVulpes | `hook/console.c` is a start | |
| Server-side anti-cheat checks | SubVulpes | `serverrecv` | |

## Security fixes (allowed during the decomp)

These change only what happens with invalid client input. The recordings have only valid input, so the harness does not change. Details: `docs/protocol.md`, section 12, item 15.

| Fix | Where | State |
| --- | --- | --- |
| Voice frame index: mask or reject the index before the `memcpy` (an index from 64 to 255 writes up to 48 KB after the voice ring) | `serverrecv` (0x33650) | Done in `hook/netguard.c` (`net_guard=1`). Not tested live with players. |
| Item count of the object part (8 bits, no cap) | `serverrecv` | Open |
| Server name copy (`strcpy`, longer than the 32 bytes sent) | | Open |
| Human health (`health + 10` in 7 bits, wraps above 117) | | Open |

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

- Mask `num_actions` and the voice frame index to 6 bits in `serverrecv` (`src/net/server.c`).
- Ignore a type 4 packet from a connection with `player_id` -1.
- Test `it < 0x400` before the write in `/phone` (`src/net/commands.c`).
- Stop the kick list at 41368 entries.
- Link `hook/netguard.c` into the standalone server, or move its checks into `serverrecv`.

