# Sub Rosa 0.24 network protocol (dedicated server <-> client)

Reference for the wire format of Sub Rosa 0.24 (protocol version **24**). Code that reads or writes it: [`tools/net/srpacket.py`](../tools/net/srpacket.py), tests in [`tools/net/test_srpacket.py`](../tools/net/test_srpacket.py).

## 1. Sources and how far to trust this

Everything here comes from reading decompiled code. **No traffic capture was available**, so nothing below was checked against a real packet. The tests prove that the reader and the writer agree with each other and with a few byte strings assembled by hand from the rules below. They do not prove that the rules match the game. Section 12 lists what is still uncertain.

| Binary | Role |
| --- | --- |
| `vanilla/0.24c_Dedicated_Linux/subrosadedicated024c` (ELF x64, symbols) | Main source. Function names and layout come from here. Addresses below are virtual addresses (image base `0x400000`). |
| `vanilla/0.24b_Dedicated_Linux/subrosadedicated024b` (ELF x64, symbols) | Same functions. Compared with the 0.24c: identical sequence of wire operations in every function listed in section 11. |
| `client/subrosa.x64` (ELF x64, symbols, build of 2015-08-02) | The 0.24b client. It contains `clientsend`, `clientrecv`, `connectmenu`, `joinmenu`, so it is the source of the handshake and of the client side. Same wire operations as the 0.24c server. |
| `blackhouser/subrosadedicated024d.exe` (PE32, no symbols) | Target of the project. Addresses are **RVAs** (module base `0x400000`, so Ghidra shows RVA + `0x400000`). Compared function by function with the ELF (section 11). |

The Black Houser 024d is the same file as the official 0.24d. Players of the group use the 0.24b client with it, and the wire format is the same (section 11).

Conventions:

- "Source" columns give the function that reads or writes the datum: `name` (ELF address / 024d RVA). The map is in section 2.4.
- `game+0x..`, `player+0x..`, `human+0x..`, `object+0x..`, `item+0x..`, `connection+0x..` are offsets in the 0.24c ELF structs (arrays `player` 0x2408 bytes per entry, `human` 0x2b64, `object` 0x8230, `item` 0x1ac, `connection` 0xf170, `event` 0x80). Where the repository already names a field (`analysis/structs/names.csv`), the name is used. Offsets in `game` shifted by 4 between 0.24b and 0.24c after `game+0x30`; the tables use the 0.24c offsets.
- "bits" fields are `pbitwrite`/`pbitread`; "bytes" fields are `cs_pwrite`/`cs_pread` (section 3).
- Servers: the three Windows/Linux server builds above behave the same on the wire. "Server" means any of them.

## 2. Transport

### 2.1 UDP

- One UDP socket (IPv4). The server binds `0.0.0.0:port` (`enablesocketserver`, ELF `0x4a4645`) and sets it non-blocking. The client uses an ephemeral port (`enablesocketclient`, ELF `0x4a471a`).
- Default port **27584** (`0x6bc0`), set by `port=` in `config.txt`. The master server fallback port is 27590 (`0x6bc6`).
- One datagram is one packet. The receive and send buffers are both **65536 bytes**. `recvpacket` takes one datagram per call; the server drains the socket every tick. There is no fragmentation or reassembly layer: a packet larger than the path MTU is fragmented by IP. The server throttles by bytes per second (section 2.5) and does not look at the MTU.
- A connection is identified only by the pair (source IPv4, source port). There is no session id, no sequence number in the header, no checksum, no encryption. The only check on receive is the 4-byte magic (section 3.1).
- Addresses in the code are kept in host order: `sendpacket(ip, port)` applies `htonl`/`htons` and `recvpacket` leaves the sender in `bsdsocket` (use `ntohl`/`ntohs`).

### 2.2 Tick

`dedicatedserverloop` (ELF `0x438db5`, 024d `0x5c8d0`) runs one simulation tick every 16 ms of system time (up to 5 catch-up ticks per pass). Each tick calls, in this order: `servercycle`, `serverrecv`, simulation, `serversend`. All "tick" counts below are these ticks (`game+0x38`, 32 bits, reset to 0 at every new game). The code assumes 60 ticks per second in its bandwidth maths.

### 2.3 Master server

Two channels, both optional for a client that already knows a server address:

1. **HTTP on startup** (`getmasterserverip`, ELF `0x4553bf` / 024d `0x1ac40`). TCP connect to `www.crypticsea.com:80` and send exactly
   `GET /anewzero/serverinfo.php HTTP/1.1\r\nHost:www.crypticsea.com\r\n\r\n`
   (it polls `send`/`recv` every 100 ms, up to 30 s each). The answer is parsed **from the start of the whole response, headers included**, as TAB-separated text: skip to the first TAB; field 1 = integer (read and ignored); field 2 = master server IPv4 as text; field 3 = master UDP port; fields 4..11 = eight strings of up to 31 characters (stored, never used by the server). If the IP is invalid the code uses `216.55.186.104:27590`.
2. **UDP to the master server** (`masterserver` IP and port from step 1, same socket and same `7DFP` header):

| Type | Direction | When | Body |
| --- | --- | --- | --- |
| `0x20` heartbeat | server -> master | `servercycle` (ELF `0x48b049` / 024d `0x12c90`), when `(tick & 0x3ff) == 0` (every 1024 ticks, about 17 s) and the server is on | none |
| `0x21` list request | client -> master | `getonlineserverlist` (client `joinmenu`), repeated while the list screen is open | none |
| `0x21` list reply | master -> client | answer to the request | `count` (int32), then `count` times `ip` (uint32, host order) + `port` (uint16), all little-endian raw bytes. The client rejects `count >= 0x2000`. |

The master server was never seen answering; the reply layout comes from the client only. The dedicated server never reads anything the master sends. Whether `crypticsea.com` still answers is unknown.

### 2.4 Function map

| Function | 0.24c ELF | 024d RVA | Role |
| --- | --- | --- | --- |
| `recvpacket` | `0x4a47fd` | `0xd860` | `recvfrom` into the 65536-byte buffer, check magic, set read cursor to 4 |
| `sendpacket` | `0x4a4925` | `0xd910` | round the cursor up to a whole byte, `sendto` the buffer |
| `resetsendpacket` | `0x4a48f7` | inlined | cursor to 0 and write the magic |
| `cs_pwrite` / `cs_pread` | `0x4a49d5` / `0x4a4a63` | `0xd990` / `0xd9f0` | byte-aligned raw copy |
| `pbitwrite` / `pbitread` | `0x4a4b14` / `0x4a4c6c` | `0xda60` / `0xdb20` | bit fields |
| `pbitwritenego` / `pbitreadnego` | `0x4a5038` / `0x4a505f` | `0xddb0` / `0xddd0` | bit field, all ones = -1 |
| `packangle` / `unpackangle` | `0x438724` / `0x4387e9` | `0x16680` (pack only) | 1 + (n-1)-bit angle |
| `packscalar` / `unpackscalar` | `0x438a66` / `0x438b31` | inlined | 1 + (n-1)-bit scaled float |
| `packposition` / `unpackposition` | `0x4388d3` / `0x43892e` | `0x16790` (pack only) | three raw float32 |
| `packorientation` / `unpackorientation` | `0x4380ac` / `0x438498` | `0x15df0` (pack only) | unit vector in 3 + 2k bits |
| `serverrecv` | `0x48b20d` | `0x33650` | all packets a server receives |
| `serversend` | `0x48a2a6` | `0x4ed30` | types 5, 6, 8 |
| `clientrecv` / `clientsend` | `0x43683d` / `0x4374ae` | - (server build) | types 1, 5, 6, 8 / type 4 |
| `connectmenu` / `joinmenu` | client ELF `0x4df77c` / next function | - | join handshake, server browser (types 0, 1, 2, 0x21) |
| `packobjectpacket` / `unpackobjectpacket` | `0x46f1ff` / `0x46ba34` | `0x20680` (pack only) | object part of type 5 |
| `packevent` / `unpackevent` | `0x43ec2d` / `0x43fd10` | `0x16f10` (pack only) | one event |
| `calculateplayervoice` | `0x4a0e9b` | `0x3cfe0` | chooses who is heard (type 8) |
| `createevent_*` | `0x43d5e5` .. `0x43ea9e` | see section 8.1 | fill the event ring |
| `player_addaction_menu/chat/item` | `0x4863f1` / `0x486211` / `0x48653a` | - | client action ring |
| `getmasterserverip` | `0x4553bf` | `0x1ac40` | master server HTTP |
| `packobjectpacket2` | `0x46dd09` | - | **dead code** in the 0.24c: no caller. Ignore. |
| `pbitwritescale` / `pbitreadscale`, `pbitwritesign` / `pbitreadsign` | `0x4a4dae`.. | - | **unused**: no caller in server or client. Ignore. |


### 2.5 Timing and throttling

- **Idle timeout.** `serverrecv` increments `connection+0x1c` once per tick for every connection. Receiving a join (type 2) or an input (type 4) packet from that (ip, port) resets it to 0. At 1200 (`0x4b0`) the connection is dropped, its player is deleted and the message `"<name> Exited"` is sent. A disconnect packet (type 7) sets the counter to 3000, so the next tick drops it. A connection whose player is gone, or whose account has `ban time > 0` (`account+0x50`), is dropped the same way.
- **Client timeout.** The client counts `clientrecv` calls without a type 5 or 6 packet and gives up after 1200 (`client+0x28 > 0x4af`).
- **Bandwidth.** `servercycle` computes `rate = servermaxbytespersecond / numofconnections` (default `servermaxbytespersecond` = 100000) and, each tick, lowers `connection+0x14` (a byte credit) by `rate / 60`, never below `-rate / 60`. `serversend` sends to a connection only if its credit is below 1, and adds the size of what it sent (type 5 + type 8). So the server sends to a client about every tick while the stream is below the limit, and skips ticks when above it. The packet is never cut to fit; the object part is simply emptied when the client is more than 23 events behind (section 6.6).
- **Kick list.** `serverrecv` drops every datagram whose source IP is in `kickedip` (checked before the type byte is read).

## 3. Datagram and bit stream

### 3.1 Header

Every datagram starts with the 4 bytes `37 44 46 50` (ASCII `7DFP`, global `socketid`, ELF `0x6ac008`, 024d RVA `0x60d08`), then the **type byte**. The receiver drops any datagram whose first 4 bytes differ, and treats a datagram shorter than 5 bytes as garbage. The same magic is used by the master server packets.

`resetsendpacket` writes the magic with `cs_pwrite`, so the bit cursor starts at byte 4, bit 0. `recvpacket` sets the read cursor to byte 4, bit 0.

### 3.2 Two kinds of field

The buffer has a **byte cursor** and a **bit cursor** (0..7) inside the current byte.

- `pbitwrite(value, n)` / `pbitread(&value, n)` (`n` up to 32): bits are packed **least significant bit first**. The low bit of the value goes to bit `cursor` of the current byte, the next bit above it, and so on; when a byte is full the cursor moves to the next byte. `pbitwrite` first clears the target bits, then ORs the value, so a field can be rewritten in place (`serversend` does this for the event count). The value is masked to `n` bits, so a negative number is written as its two's complement low bits.
- `cs_pwrite(ptr, size, count)` / `cs_pread(ptr, size, count)`: if the bit cursor is not 0, the cursor first moves to the start of the **next byte**; then `size * count` raw bytes are copied. Integers and floats are the raw little-endian x86 memory (int32, uint32, IEEE float32).

Consequences:

- A run of bit fields that does not end on a byte boundary leaves **padding bits** (zero) up to the next byte, but only when a raw field or the end of the packet follows.
- `sendpacket` sends `byte cursor + 1` bytes if the bit cursor is not 0, else `byte cursor`.
- Example, 3 + 2 + 10 bits: `101` `11` `1010101011` pack into stream value `0b1010101011_11_101`; bytes `0x7D 0x55` (stream value `0x557D`) with bit 7 of the second byte unused.

Limits in the original (kept in `srpacket.py`):

| Rule | Code |
| --- | --- |
| Write fails (returns 0, writes nothing) if the byte cursor is already >= 65536 | `pbitwrite`, `cs_pwrite` |
| `pbitread` fails if `length < read_byte + (n >> 3)`. It does **not** include the leftover bits, so it may read up to 7 bits past the end of the datagram (stale bytes from the previous packet) | `pbitread` |
| `cs_pread` fails if `length < read_byte + size*count`, **checked before aligning**, so it may read 1 byte past the end | `cs_pread` |
| `serverrecv` ignores the result of nearly every read, so a short packet is parsed as if the missing data were stale bytes | `serverrecv` |

### 3.3 Field types used in the tables

| Type | Wire | Meaning |
| --- | --- | --- |
| `u<n>` | `n` bits | unsigned |
| `nego<n>` (`pbitwritenego` / `pbitreadnego`) | `n` bits | all ones reads as **-1** (`none`). Writing -1 gives all ones. |
| `i32`, `u32`, `f32`, `vec3` | raw bytes | int32 / uint32 / float32 / three float32 (12 bytes) |
| `angle<n>` (`packangle`) | `n` bits | sign bit (`1 << (n-1)`) + magnitude of `n-1` bits: `m = trunc(|x| * 2^(n-1) / 2pi)`, **masked** to `n-1` bits (it wraps at 2 pi). Decoding: `m / (2^(n-1) / 2pi)` with the sign. `2pi` is the double `6.28318530718`. Step with 16 bits: 0.00019 rad; with 8 bits: 0.049 rad. |
| `scalar<n>(scale)` (`packscalar`) | `n` bits | same sign + magnitude layout, `m = trunc(|x| * 2^(n-1) / scale)` **clamped** to `2^(n-1)-1`. All uses have `scale = 1.0`. |
| `orient<n>` (`packorientation`) | `n` bits (31 or 21) | unit vector, section 3.4 |

### 3.4 Orientation (octahedral subdivision)

`packorientation(v, n)` encodes a unit vector with an octahedron whose six vertices are `(0,-1,0) (-1,0,0) (0,0,-1) (1,0,0) (0,0,1) (0,1,0)` (table `octahedron`, ELF `0x6ab540`):

1. The low 3 bits pick one of 8 faces: `bit0 = x < 0`, `bit1 = z < 0`, `bit2 = y < 0`. The face vertices `a b c` are `octahedronindex[face]` (ELF `0x6ab5a0`): `(5,3,4) (5,4,1) (5,2,3) (5,1,2) (4,3,0) (1,4,0) (3,2,0) (2,1,0)`.
2. For `i = 3, 5, 7, ... < n` the triangle is split at the normalized edge midpoints `t0 = norm(a+b)`, `t1 = norm(b+c)`, `t2 = norm(c+a)`. Two bits `d` at bit `i` choose the sub-triangle: `0` -> `(a, t0, t2)`, `1` -> `(t0, b, t1)`, `2` -> `(t2, t1, c)`, `3` -> `(t0, t1, t2)`.
3. The decoder returns `norm(a + b + c)` of the final triangle.

31 bits give 14 refinements, 21 bits give 9. The encoder picks `d` with three sign tests of `crossproduct(p - t, e) . p`. The game's `crossproduct(a, b)` is the **opposite of the usual `a x b`** (it computes `b x a`); `srpacket.py` copies this.

## 4. Packet types

All types are one datagram. The first byte after the magic is the type.

| Type | Name | Direction | Sent by / read by |
| --- | --- | --- | --- |
| 0 | query | client -> server | `joinmenu` / `serverrecv` |
| 1 | query reply | server -> client | `serverrecv` / `joinmenu` |
| 2 | join | client -> server | `connectmenu` / `serverrecv` |
| 4 | input | client -> server | `clientsend` / `serverrecv` |
| 5 | state | server -> client | `serversend` / `clientrecv` |
| 6 | new game | server -> client | `serversend` / `clientrecv`, `connectmenu` |
| 7 | disconnect | client -> server | `gameloop` (on exit) / `serverrecv` |
| 8 | voice | server -> client | `serversend` / `clientrecv` |
| 0x20 | heartbeat | server -> master | `servercycle` |
| 0x21 | list request / reply | client <-> master | `getonlineserverlist` / `joinmenu` |

Types 3, 9..0x1f and 0x22+ are not used. `serverrecv` ignores a type it does not know.

### 4.1 Lifecycle

```
client                                    server
  | type 0 query  (every 100 ms, per server, in the list)  ->
  | <-  type 1 query reply (name, players, account key)
  | type 2 join  (every 100 ms until a type 6 arrives)  ->      creates connection + player
  | <-  type 6 new game (game id)           (every tick the credit allows, until echoed)
  |   ... client loads the level ...
  | type 4 input (game id echo, inputs, acks)  ->  (each client frame)
  | <-  type 5 state + type 8 voice                (each tick the credit allows)
  | type 7 disconnect  ->                                         (or the idle timeout)
```

The "game id" (`game+0x1c`) is a counter incremented at every `resetgame` (new round or map). The server sends a type 6 to a connection whose `connection+0x8` (last game id the client echoed) differs from the current id, and type 5 otherwise. While a join is new, `connection+0x8` is -1, so the client gets type 6 first. The client sets the game id from the type 6 and echoes it in every type 4.

## 5. Handshake packets

### 5.1 Type 0, query (client -> server)

| Order | Field | Size | Meaning | Source |
| --- | --- | --- | --- | --- |
| 1 | `version` | u8 (bits) | client protocol version (24) | `joinmenu`, `serverrecv` |
| 2 | `nonce` | 4 bytes | opaque, echoed (the client sends `SDL_GetTicks()` and computes the ping from the echo) | same |

### 5.2 Type 1, query reply (server -> client)

| Order | Field | Size | Meaning | Source |
| --- | --- | --- | --- | --- |
| 1 | `version` | u8 | `currentversion` of the server (24) | `serverrecv` type 0 branch |
| 2 | `nonce` | 4 bytes | copy of the query's | same |
| 3 | `gametype` | u4 | `game+0x8` (3 = normal round game, 4 = also accepted by the config; 0..2 single player) | same |
| 4 | `players` | u8 | connected non-bot players (`server+0x64`) | same |
| 5 | `max_players` | u8 | `maxnumofplayers=` clamped to 2..128 (`server+0x5c`) | same |
| 6 | `name` | 32 bytes | server name (`servername=`), copied raw from `server+0x14` | same |
| 7 | `account_key` | uint32 | first dword of the `account` array (a per-server key). **Only sent if the query's `version` is greater than 6** | same |

The client uses `account_key` to derive its account id for that server (`createclientaccount`). Reading is done by `joinmenu`, which stores the reply in its server list by (ip, port).

### 5.3 Type 2, join (client -> server)

| Order | Field | Size | Meaning | Source |
| --- | --- | --- | --- | --- |
| 1 | `version` | u8 | must equal the server's `currentversion` or the packet is ignored | `connectmenu`, `serverrecv` type 2 branch |
| 2 | `name` | 32 bytes | player name (`name=` from the client options, at most 30 characters). `fixplayername` (ELF/024d `0xa580`) replaces `/` with a space, trims spaces and uses `Noname` if empty. | same |
| 3 | `account_id[0]` | uint32 | `localplayer` (client account id, part 1) | same |
| 4 | `account_id[1]` | uint32 | part 2 | same |

Server behaviour: if (ip, port) is already a connection, only its idle counter is reset. Otherwise, if `players < max_players`, a player slot is created, the `(account_id[0], account_id[1])` pair selects or creates the account (`createaccount`, 024d `0xded0`), and `"<name> Joined"` is broadcast (a message event). A second connection for the same account takes over the first one's player. A banned account (`account+0x50 > 0`) and a full server are ignored **silently**: nothing is sent back. There is no password check at join (`serverpassword=` is the admin password of `/admin`).

### 5.4 Type 6, new game (server -> client)

| Order | Field | Size | Meaning | Source |
| --- | --- | --- | --- | --- |
| 1 | `game_id` | uint32 | `game+0x1c` | `serversend` (else branch) |
| 2 | `gametype` | u4 | `game+0x8` | same |
| 3 | `game_c` | u4 | `game+0xc` (0.24c layout) | same |
| 4 | `angle_a` | angle16 | `game+0x4c` | same |
| 5 | `angle_b` | angle16 | `game+0x50` | same |

Total 14 bytes. The client resets its timeout on it. `connectmenu` accepts it as "connected".

### 5.5 Type 7, disconnect (client -> server)

No body. Sent by the client when it leaves the game loop.

## 6. In-game packets

### 6.1 Type 4, input (client -> server)

Read by `serverrecv` only if the connection exists and `game_id` equals the current game id; otherwise the rest is ignored (the idle counter is still reset when the connection is known).

| Order | Field | Size | Meaning | Source |
| --- | --- | --- | --- | --- |
| 1 | `game_id` | uint32 | the id from the last type 6 or 5 (`game+0x1c`) | `clientsend`, `serverrecv` type 4 |
| 2 | `axes[0..3]` | 4 x f32 | `player+0x7c..0x88`: movement axes (`input_axes`) | same |
| 3 | `yaw`, `pitch` | 2 x f32 | `player+0x8c`, `+0x90` | same |
| 4 | `extra[0..1]` | 2 x f32 | `player+0x94`, `+0x98` (`input_extra`) | same |
| 5 | `buttons` | uint32 | `player+0x9c` bit mask | same |
| 6 | `input_mode` | u8 | `player+0xc8` | same |
| 7 | `hud_mode` | u4 | `player+0xac` | same |
| 8 | `event_ack` | u16 | next event index the client has not processed (`eventloc`), stored in `connection+0x44` | same |
| 9 | `cam_pos` | vec3 | camera position (`connection+0xf158`), used to pick what to send | same |
| 10 | `num_actions` | u4 | 0..15 | same |
| 11 | `action_start` | u8 | ring index of the first action (`player+0xe8` on the client) | same |
| 12 | actions | `num_actions` x (below) | | same |
| 13 | voice frames | 6 x (below) | | same |
| 14 | `voice_flag` | u1 | `speex[player]+0x1920c` | same |
| 15 | `spectate` | nego8 | human being watched while dead (`connection+0x154`), -1 none | same |

Total fixed part before the actions: 61 bytes plus the bits of 10..11.

**Actions.** The client keeps a 64-entry ring (`player+0xec`, 0x54 bytes per entry, written by `player_addaction_*`; head `player+0xe4`). Each type 4 resends all actions from `action_start` up to the head (at most 15). The server stores them into its own ring starting at `action_start` and processes them in order. The server tells the client how far it has got in every type 5 (`action_ack`, section 6.2), and the client sets its ring tail to that number. The server therefore sees the same action many times only when the ack is late; the ring index makes them idempotent.

Each action:

| Order | Field | Size | Meaning |
| --- | --- | --- | --- |
| 1 | `kind` | u4 | 0 menu, 1 chat, 2 item |
| kind 0 | `menu` | u8 | menu id (compared with `player.menu`, `player+0xd4`) |
| | `value` | i32 | choice (raw 4 bytes) |
| kind 1 | `length` | u6 | text length (0..63) |
| | text | `length` x 1 byte | each byte is its own raw write, so it is aligned (`cs_pwrite(ptr,1,1)` per character) |
| | `mode` | u4 | `chat+4` of the client: stored as `range_mode` of the message event (section 8.2) |
| kind 2 | `a`, `b` | u16, u16 | item action arguments (`player_addaction_item`) |
| other | - | - | nothing else is read |

Chat text starting with `/` is a server command (`server_playermessage`: `/admin`, `/kick`, `/ban...`, `/give`, `/cash`, `/resetgame`, `/heli`, `/list1..3`, ...); it is not broadcast.

**Voice.** The client sends the 6 most recent voice frames each packet. The server reads exactly 6; for each frame `index` is read as 8 bits, `length` as 8 bits (0..255), `mode` 2 bits, then `length` raw bytes (a new byte alignment starts the data, also when `length` is 0: an empty frame takes 3 aligned bytes, checked with a real capture):

| Field | Size |
| --- | --- |
| `index` | u8 (the client writes 0..63, a ring index) |
| `length` | u8 |
| `mode` | u2 |
| `data` | `length` bytes |

The server writes the frame into its per-player voice ring at `index` and then sets the ring head to `last_index + 1` (mod 64). Voice is only used when the server has `voicechat=1`.

### 6.2 Type 5, state (server -> client)

One per connection per tick (credit permitting). Written by `serversend`, read by `clientrecv`. The client ignores a type 5 whose `game_id` differs from its own, or whose `tick` is older than the last one by less than 100 ticks (`(game+0x38 <= tick) || (game+0x38 - tick > 99)`).

| Order | Field | Size | Meaning | Source |
| --- | --- | --- | --- | --- |
| 1 | `game_id` | uint32 | `game+0x1c` | `serversend`, `clientrecv` |
| 2 | `tick` | uint32 | `game+0x38` server tick | same |
| 3 | `phase` | u4 | `game+0x20`: 1 = waiting before the round, 2 = round running, 3 = round over | same |
| 4 | `ready[0..31]` | 32 x u1 | `player+0x3c` of slots 0..31. **Only present when `phase == 1`** | same |
| 5 | `phase_timer` | u16 | `game+0x24`: ticks left in the phase (65535 max; a long round time overflows it, section 12) | same |
| 6 | `world` | u30 | first dword of `game_world` | same |
| 7 | `team_counts[0..2]` | 3 x u6 | `game+0x90`, `+0x120c`, `+0x2388`: players per team | same |
| 8 | `player` | u8 | slot of the receiving player (`view`) | same |
| 9 | `human` | nego10 | the player's human (`player+0x78`), -1 none | same |
| 10 | `look_yaw`, `look_pitch` | 2 x angle16 | `player+0xa4`, `+0xa8`: copied from the human | same |
| 11 | `cam_pos` | vec3 | eye position (`human+0x2a0`), zero if no human | same |
| 12 | `menu` | u8 | `player+0xd4` | same |
| 13 | `menu_building` | u16 | `player+0xd8` | same |
| 14 | shop list | see below | **only when `menu == 10`** | same |
| 15 | `money` | i32 | `player+0x40` | same |
| 16 | `action_ack` | u8 | `player+0xe4`: actions the server has received (ring head); the client stores it as its ring tail | same |
| 17 | `health` | u8 | `player+0x15ec`: human health, never negative | same |
| 18 | `unk_15f0` | u6 | `player+0x15f0` (not written anywhere by the server logic) | same |
| 19 | `unk_15f4` | u8 | `player+0x15f4` (same) | same |
| 20 | inventory | 7 x (u4 count + count x u16) | for each of the 7 slots: item type in the low 8 bits, `0x100` = usable ammo/charge, `0x200` = ..., ammo in bits 8+ for the first two slots (built in `playersimulation`) | same |
| 21 | objects part | section 6.3 | humans, objects, items, city objects | `packobjectpacket` |
| 22 | `event_total` | u16 | server `numofevents` (next event index to be created) | same |
| 23 | `num_events` | u6 | number of events that follow (the server sends 0..31) | same |
| 24 | `first_event` | u16 | index of the first event (`connection+0x44`) | same |
| 25 | events | `num_events` x event | section 7 | `packevent` |

Shop list (when `menu == 10`): `count` u8 = number of items of building `menu_building` (`city+0x302a4 + building*0x9dc`), then `count` times `u8`, `u24`, `u4` (items of that building: three fields of 12 bytes in the city table).

The count in 23 is written first and patched after the events are packed, because `packevent` may write fewer events than planned; the value the server finally writes equals the number of events that follow (`local_30`), fillers included.

### 6.3 Object part of type 5

Four lists, each starting with a **u8 count**. Written by `packobjectpacket` (ELF `0x46f1ff`, 024d `0x20680`), read by `unpackobjectpacket` (ELF `0x46ba34`).

**Humans** (at most 8 per packet; the server picks by a distance-based priority and gives its own player's human a fixed top priority):

| Field | Size | Meaning |
| --- | --- | --- |
| `id` | u10 | human index (0..127 are scanned) |
| `kind` | u1 | 0 = full body (always 0 in 0.24); if 1 nothing follows |
| `vehicle` | nego8 | `human+0x14`: object the human is in, -1 none |
| `seat` | u4 | `human+0x18` |
| `health+10` | u7 | `human+0x28 + 10`, clamped to >= 0 (the client subtracts 10). Overflows above 117 |
| `bleeding` | u1 | `human+0x2b1c` |
| bone 0 position | vec3 | `human+0xc8` |
| bone 0 orientation A | orient31 | `human+0x104` |
| bone 0 orientation B | orient31 | `human+0x110` |
| bones 1..14 | 3 x angle16 each | `human+0x198, +0x19c, +0x1a0` of the bone (stride 0xe0): joint angles. **Only bones 1..8 when `vehicle != -1`** (the loop is `bone < 9`) |

The client rebuilds each bone position from its parent bone and the three angles (`body` table).

**Objects** (vehicles; at most 16 per packet: the 2 highest priority get mode 0, the next 6 mode 1, the next 8 mode 2; objects in state 3 are skipped):

| Field | Size | Meaning |
| --- | --- | --- |
| `id` | u10 | object index |
| `mode` | u2 | 0 = full, 1 = reduced orientation, 2 = position only |
| `pos` | vec3 | `object+0x24` |
| *if mode is 0 or 1:* orientation A, B | orient31 (mode 0) or orient21 (mode 1) | `object+0x48`, `+0x54` |
| 6 angles | 6 x angle8 | `object+0x5f1c, +0x5fb4, +0x604c, +0x60e4, +0x5f20, +0x5fb8`: for the first four wheels (stride 0x98) the first field, and a second field for the first two wheels (rotor angles in helicopters) |
| 8 scalars | 8 x scalar6(1.0) | `object+0x5f48, +0x5fe0, +0x6078, +0x6110, +0x5f5c, +0x5ff4, +0x608c, +0x6124`: per-wheel values |
| velocity | 3 x scalar8(1.0) | `object+0x60, +0x64, +0x68` |
| `flags` | u13 | `object+0x5ee0` |

**Items** (loose items; the server sends an item every 4th packet, or every 32nd when it is far or at rest, or always when it is within 32 m; no explicit limit except the 8-bit count):

| Field | Size | Meaning |
| --- | --- | --- |
| `id` | u10 | item index |
| `pos` | vec3 | `item+0x48` |
| orientation A, B | 2 x orient31 | `item+0x90`, `+0x9c` |
| `has_cash` | u1 | item type is `0x10` (cash) |
| *if cash:* | u4, u4, u30 | `item+0x138`, `+0x134`, `+0x13c` (bill counts and mask) |

**City objects** (scenery objects that can move: all active ones, up to 128):

| Field | Size | Meaning |
| --- | --- | --- |
| `id` | u10 | |
| `pos` | vec3 | `cityobject+0x8` |
| orientation A, B | 2 x orient31 | `+0x2c`, `+0x38` |
| `a` | u7 | `+0x4c` |
| `b` | u7 | `+0x48` |
| `scalar` | scalar6(1.0) | `+0x50` |

**Throttle.** If the client is more than 23 events behind (`numofevents - connection+0x44 > 0x17`, modulo 65536), the three lists humans, objects and items are sent empty (count 0) for that packet. City objects are still sent.

### 6.4 Type 8, voice (server -> client)

Sent right after every type 5, in its own datagram. Body only bits, 8 slots:

| Field | Size |
| --- | --- |
| `active` | u1 |
| *if active:* `human` | nego8 (speaking human, or -1) |
| *if active:* `item` | nego8 (radio item used, or -1) |
| *if active:* 4 frames | each `index` u6, `length` u8, `mode` u2, `data` `length` bytes (aligned) |

A packet with no active slot is `7DFP 08 00`. `calculateplayervoice` chooses the slots: speakers within 256 units (or 64 through a radio item when the two radios are linked and on), with a free slot, skipping players who are muted.

### 6.5 Event resend

Events are written into a ring of 65536 entries (`event`, 0x80 bytes each). `serversend` sends, per tick and per connection, the events from `connection+0x44` (the client's last `event_ack`) up to `numofevents`, at most 31, and **repeats them every tick until the client acks**. So the same event is delivered several times; the client applies each index once (it only advances when `numofevents == index`).

### 6.6 What the server leaves out

- `packevent` replaces an event by a 6-bit **filler** (type `0x12`, no tick, no body) when: it is a message (type 2) not attached to the newspaper (`attach_type != 3`) or a sound (type 9) and is older than 600 ticks; or it is an admin-only message (`attach_type == 4`) and the connection is not an admin (`connection+0xc == 0`).
- For non-admin connections, a message attached to a human (`attach_type 1`) or an item (`attach_type 2`) is dropped (filler) when the receiver is too far from the speaker: `score = 32 / (distance * 0.125)`, multiplied by 0.25 for `range_mode 0`, by 2 for `range_mode 2`, and by 0.25 more for item-attached messages; below 4.0 it is dropped. With `range_mode 1` the range is 64 units; 16 for 0; 128 for 2.
- When a message is attached to the receiver's own human and the chat filter replaced the player's text, the server sends the **original** text (`player+0x23c8`) to that player, and the filtered text to everybody else (`packevent`, first branch).

## 7. Events

A type 5 packet carries events in this form. `unpackevent` (ELF `0x43fd10`) and `packevent` (ELF `0x43ec2d`, 024d `0x16f10`) are mirror images.

```
type  u6
if type == 0x12 (filler): end (6 bits in total)
tick  u28     server tick (game+0x38) at creation
body  by type (below)
```

Types 0..17 are produced; any other type except the filler has no body (the reader would read type and tick only).

| Type | Producer (`createevent_*`) | Body (wire order) |
| --- | --- | --- |
| 0 | `bullet` | `weapon` u4, `shooter` u10, `pos` vec3, `vel` vec3 |
| 1 | `bullethit` | `kind` u4, `target` u6, `pos` vec3, `normal` vec3 (zero if none) |
| 2 | `message` | `length` u6, `attach_type` u4, `attach_id` nego10, `range_mode` u4, then `length` x u7 characters (7-bit ASCII, no terminator). At most 63. |
| 3 | `createobject` | `object` u10, `object_type` u8, `variant` nego4 (`object+0x18`) |
| 4 | `updateobject` | `object` u10, `arg_a` u4, `arg_b` u10, `vec_a` vec3, `vec_b` vec3 |
| 5 | `updateitem` | `item_key` u20, `parent_human` nego10, `parent_item` nego10, `parent_slot` u4 |
| 6 | `updateiteminfo` | `item` u10, `call_state` u3, `info` u16, `info2` u2 |
| 7 | `updateplayer` | `packed` u16, `human` nego10, `money` i32, `look` u16, then 31 x u7 name characters (always 31; the server copies the player's name up to the NUL and beyond it) |
| 8 | `updateplayer_finance` | `player` u8, `money` i32, `stocks` i32 |
| 9 | `sound` | `sound` u8, `pos` vec3, `volume` f32, `pitch` f32 |
| 10 | `updatedoor` | `door` u8, `open` u1 |
| 11 | `updatescenario` | four i32 |
| 12 | `updatemission` | four i32 |
| 13 | `updatestocks` | three i32 |
| 14 | `updatecarmarket` | u8, u8, u8, u20 |
| 15 | `updateblock_interior` | three i32 |
| 16 | `updateblock_quad` | i32, i32, i32, nego10, vec3, vec3 |
| 17 | `updatehuman` | `human` u10, `active` u1, `player` nego8 |
| 0x12 | (filler) | none, and no tick |

Packed fields:

- Type 2 `attach_type`: 0 chat of a player (`attach_id` = player slot), 1 speech of a human (`attach_id` = human), 2 radio (`attach_id` = item), 3 newspaper line (`attach_id` = line, `range_mode` = 0/1), 4 admin console text (`attach_id` -1). `range_mode` (4 bits) is the chat mode chosen by the speaker.
- Type 5 `item_key` = `type << 11 | active << 10 | item` (`item` 10 bits, `type` 9 bits).
- Type 7 `packed` = `(team + 1) << 10 | active << 9 | is_bot << 8 | player`. `look` = `look_a` (`player+0x23b4`, bits 0..1) + `look_b * 4` (`+0x23b8`, bits 2..4) + `look_c * 0x20` (`+0x23bc`, bits 5..6) + `player+0x23c0 * 0x80` + `player+0x23c4 * 0x100`.
- Type 12 and type 11 pack several 16-bit halves in each i32 (`createevent_updatemission` / `_updatescenario`); the pieces are not documented here.
- Type 14 `d` is `market+0x140 + index * 0x10` (u20).

### 7.1 Where the 024d creates them

`createevent_*` functions in the 024d, by reading the code (the repository's `functions.csv` has only some of them; two of its names are wrong, see section 12):

| Type | 024d RVA |
| --- | --- |
| 0 `bullet` | `0x16bb0` |
| 1 `bullethit` | `0x16c40` |
| 2 `message` | `0x6260` |
| 3 `createobject` | inlined in `0x31800` |
| 4 `updateobject` | `0x16ce0` |
| 5 `updateitem` | `0x6380` |
| 6 `updateiteminfo` | `0x6420` |
| 7 `updateplayer` | `0x64a0` |
| 8 `updateplayer_finance` | `0x65f0` |
| 9 `sound` | `0x16db0` |
| 10 `updatedoor` | `0x6660` |
| 12 `updatemission` | `0x6760` |
| 13 `updatestocks` | `0x6840` |
| 14 `updatecarmarket` | `0x68d0` |
| 16 `updateblock_quad` | `0x16e30` |
| 17 `updatehuman` | inlined in `0x50ae0` (`createhuman`), `0x33650` (`serverrecv`) and `0x18720` |
| 11, 15 | no producer found in the 024d, but `packevent` (`0x16f10`) still writes them with the same layout |

The event ring (`event`) in the 024d is at RVA `0x95dd880` (0x80 bytes per entry), `numofevents` at `0xf437318`.

## 8. Server-side data used by the wire code

Fields of `connection` (0xf170 bytes per entry) that the protocol touches:

| Offset | Field | Meaning |
| --- | --- | --- |
| `+0x0` | `ip` | host order |
| `+0x4` | `port` | |
| `+0x8` | `game_id` | last game id echoed by the client; -1 for a new join |
| `+0xc` | `is_admin` | set by `/admin <password>` |
| `+0x10` | `player_id` | -1 when the player was removed |
| `+0x14` | `credit` | bandwidth credit (section 2.5) |
| `+0x18` | `rate` | bytes per second for this connection |
| `+0x1c` | `idle` | ticks since the last join or input |
| `+0x44` | `event_ack` | first event to send |
| `+0x4c` | `sequence` | counter of packets sent; selects which items are sent |
| `+0x54 + k*0x20` (k < 8) | voice slots | `+0x0` active, `+0x4` speaker player, `+0x8` human, `+0xc` item, `+0x1c` distance |
| `+0x154` | `spectate` | human watched while dead |
| `+0x158 + i*0x1c` | object priorities | accumulators for objects |
| `+0x7158 + i*0x1c` | human priorities | accumulators for humans |
| `+0xf158` | `camera_pos` | vec3, from type 4 |
| `+0xf164` | `listener_pos` | vec3, where the player's head is |

## 9. Version comparison

The wire operations of these functions are the same, in the same order and with the same sizes, in all three programs that have the code: `serverrecv`, `serversend`, `clientsend`, `clientrecv`, `packevent`, `unpackevent`, `packobjectpacket`, `unpackobjectpacket`, `packangle`, `unpackangle`, `packorientation`, `unpackorientation`, `packscalar`, `unpackscalar`, `pbitwrite`, `pbitread`, `servercycle`, `calculateplayervoice`.

- 0.24b client (`subrosa.x64`) vs 0.24c server ELF: identical wire operation sequences. Differences are only `game` offsets (0.24c inserted a dword after `game+0x30`) and debug code in the client.
- 0.24b server ELF vs 0.24c server ELF: identical.
- 024d vs 0.24c: compared by reading `serverrecv` (types 0, 2, 4), `serversend` (types 5, 6, 8), `packevent` (all types), `packobjectpacket`, the bit functions and the `createevent_*` creators. Same field order and sizes, same magic, `currentversion` = 24. MSVC inlines `cs_pwrite`/`cs_pread` and the angle/scalar packers, so the code looks different but the operations are the same.

No difference between versions was found. That does not exclude a difference in a part that was not compared. The logic that chooses what to send (priorities, item selection) was read in the 0.24c only; in the 024d it was compared just by the order and size of what it writes.

## 10. Using `srpacket.py`

```python
import sys; sys.path.insert(0, "tools/net")
import srpacket as sr

raw = sr.build_packet(sr.Join(24, b"Agent 7", (1, 2)))     # bytes of the datagram
pkt = sr.parse_packet(raw)                                   # -> sr.Join(...)
```

Classes: `Query`, `QueryReply`, `Join`, `NewGame`, `Disconnect`, `Input` (+ `Action`, `VoiceFrame`), `State` (+ `HumanBlock`, `ObjectBlock`, `ItemBlock`, `CityObjectBlock`, `Event`), `Voice` (+ `VoiceSlot`), `Heartbeat`, `ListRequest`, `ListReply`. Lower level: `PacketWriter` / `PacketReader` (`write_bits`, `read_bits`, `write_bytes`, `read_bytes`, `tell`, `seek`), `pack_angle`, `pack_scalar`, `pack_orientation` and their inverses.

Command line: `python -I tools/net/srpacket.py dump <hex string or file>` prints a parsed packet; `selftest` builds one and parses it. Tests: `python -I tools/net/test_srpacket.py`.

Quantized floats are lossy. A parsed value can move by one step if you build the packet again, as in the original.

## 11. Verification summary

| What | How | Result |
| --- | --- | --- |
| Bit order and cursor rules | `pbitwrite`, `pbitread`, `cs_pwrite`, `cs_pread` decompiled from the ELF and the 024d | same logic in both |
| Field order | decompiled readers and writers of each packet | read order = write order for every pair that exists on both sides |
| Client 0.24b = server 0.24c = 024d | sequence of wire calls compared by script (ELF/ELF) and by reading (ELF/024d) | no difference |
| Hand-assembled bytes | `test_srpacket.py` (query, reply, join, new game, input offsets, list reply, event bits, voice) | pass |
| Live traffic | none | **not done** |

## 12. Uncertain parts

Wire format that is certain but whose meaning is not, and parts not checked:

1. **Capture (checked).** A 35-minute play session on the 024d with 0.24b clients was captured by the hook DLL (`capture_packets=1`, `tools/net/capture.py`). `srpacket.py` reads all of it with no error: 48229 state packets (type 5, 53 MB), the input packets (type 4), voice (type 8), query, join, new game, disconnect and heartbeat. The capture found one error in this document, now fixed: an empty voice frame still aligns to the next byte.
2. **`game+0xc`** (u4 in types 6): meaning unknown. It appears inside the `updatescenario` event too, so it may be a scenario or mode.
3. **`game+0x4c`, `game+0x50`** (the two angles of type 6): probably the sun or light direction. Not verified.
4. **`phase_timer` is 16 bits** but the round time is up to `30 * 3600` ticks (`roundtime=` is clamped to 30 minutes), above 65535. It probably wraps for rounds over 18 minutes. The wrap was not tested.
5. **`player+0x15f0` (6 bits) and `+0x15f4` (8 bits)** in type 5: no code writes them, so they are probably always zero. Meaning unknown.
6. **Inventory entries**: the high bits (`0x100`, `0x200`, bits 8+ shifted ammo) come from `playersimulation`; their exact meaning (ammo, loaded, magazine) was not traced.
7. **`world`** (30 bits, first dword of `game_world`): meaning unknown (maybe a clock).
8. **`input` fields**: `axes[0..3]`, `extra[0..1]`, `buttons` bit meanings, `input_mode`, `hud_mode` are only known by offset. `buttons` bits 0, 3, 4 are tested by `humansimulation`.
9. **Item action** (`kind 2`, two u16): the meaning of `a` and `b` was not traced.
10. **Event meanings** with generic names in the table: `bullet.weapon/shooter`, `bullethit.kind/target`, `updateobject.arg_*`, `createobject.variant`, `updateiteminfo.info*`, `sound.volume/pitch` (the two floats), `updateplayer.look` upper bits, `updatecarmarket`, `updatescenario`, `updatemission` and `updateblock_*` payloads.
11. **Object block**: the 6 angles and 8 scalars are per-wheel values at stride `0x98`; which wheel and which role (steer, spin, suspension) was not traced. `flags` (13 bits) is `object+0x5ee0`.
12. **Master server**: the UDP list reply layout comes from the client only; the HTTP answer fields 1 and 4..11 are read but their meaning is unknown; the server `crypticsea.com` may no longer exist. The heartbeat has no known acknowledgement.
13. **Types never produced**: `HumanBlock.kind == 1`, `ObjectBlock.mode == 3`, events 11 and 15 (not produced by the 024d), `packobjectpacket2` (dead code), `pbitwritescale`/`pbitwritesign` (unused). The parser handles them as the code does.
14. **Voice codec**: the frames sit in an array called `speex`; the codec, frame duration and the meaning of `mode` (2 bits) were not checked.
15. **Out-of-range inputs in the original**, relevant to anyone who writes a compatible server:
    - the voice `index` of a type 4 packet is read as 8 bits and used to index a 64-entry ring before it is masked; a value >= 64 writes outside the ring in the original server; the hook DLL masks it to 6 bits before the game reads the packet, and drops a type 4 packet that ends before its last voice frame (`hook/netguard.c`, `net_guard=1` by default);
    - the item count of the object part is 8 bits but the item loop has no explicit cap (up to 1024 items), so a count above 255 would wrap;
    - the server name is copied with `strcpy` into `server+0x14`, but only 32 bytes are sent, so a long `servername=` (up to 49 characters) is cut in the packet and the unsent part is still written into memory after the name;
    - the human health is sent as `health + 10` in 7 bits and wraps above 117.
16. **Names in `analysis/024d/functions.csv`** that look wrong after reading the code: `0x16bb0` is named `getrandomvector2d` but is `createevent_bullet`; `0x16db0` is named `scaleaddvectors` but is `createevent_sound`. This document does not edit that file.
17. **Reconnect and account take-over**, bans and "server full" are described from `serverrecv`; the effect on a real client (what it shows when ignored) was not observed.
18. **Re-encoding**: `srpacket.py` uses truncation like the original. A parsed angle encoded again can differ by one step. Bytes of a captured packet therefore may not be reproduced bit for bit by parse + build for the quantized fields (they are for all integer fields).
