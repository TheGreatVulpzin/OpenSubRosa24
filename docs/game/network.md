# Network

The network code has these parts: the packet code reads and writes the bit stream of one UDP packet, the event code makes the server events, and the server code receives from and sends to the clients. Files: `src/net/packet.c`, `events.c`, `server.c`, `netpack.c`, `commands.c`, `master.c` and `client.c`. The wire format is in [protocol.md](../protocol.md). This page does not repeat it.

## Records

| Record | Key fields | Use |
| --- | --- | --- |
| `BsdSocket` (global `sr_bsdsocket`) | `current`, `enabled[8]`, `handle[8]`, `addr`, `addr_len` | The sockets and the address of the next send. |
| | `send_buf`, `send_size`, `send_bit` | The send buffer (65536 bytes). `send_size` is the byte position. `send_bit` is the bit position in that byte. |
| | `recv_buf`, `recv_size`, `recv_pos`, `recv_bit` | The receive buffer, with the same cursor layout. |
| `Event` (array `sr_events`, 65536 records of 0x80 bytes) | `type`, `tick`, `pos`, `vec`, `param`, `attach_type`, `attach_id`, `range_mode`, `sound_a`, `sound_b`, `message` | One event. Its meaning depends on `type`. |
| `sr_numofevents` | index 0 to 0xffff | The next free event record. |

## Packet flow

A packet is a bit stream. The first 4 bytes are the socket id (`7DFP`).

1. `recvpacket` reads one datagram from the current socket. A datagram that does not start with the socket id counts as 0 bytes. The read cursor then starts at byte 4, bit 0.
2. `pbitread` takes bits from the receive buffer, low bit first. It fails when the packet has too few bytes.
3. The sender fills the send buffer with `pbitwrite`, `cs_pwrite`, `packangle`, `packposition` and `packorientation`.
4. `sendpacket` aligns the cursor to a whole byte. It then writes the host address and port into the address record and calls `os_sendto`.

Rules of the bit stream:

- `pbitwrite` writes the low bits of a value. It clears the target bits first, so a field can be written again in place.
- A count of 32 bits writes zeros. The x86 uses only the low 5 bits of a shift count.
- `cs_pwrite` and `packposition` first move to the next whole byte. They then copy raw bytes.
- `pbitwritenego` writes the same as `pbitwrite` and always returns 1.
- A write fails when the send buffer has 65535 bytes or more.

### Angles and orientations

- `packangle` writes `bits - 1` bits of size and a sign bit in the top bit. The size is the angle scaled by `2^(bits-1) / 2pi`. The value wraps at 2 pi.
- `packposition` writes three raw floats.
- `packorientation` writes a unit vector as a path in an octahedron. The first 3 bits give the face (8 faces). Each 2 bits after that split the current triangle in 4 parts. The code keeps the triangle and the midpoints of its edges in a game global (`g_tri`).

## Event flow

Only the server makes events. Each `createevent_*` function does the same steps:

1. Return at once when `sr_server` is 0.
2. Take the record at `sr_numofevents`. Write `type` and the tick (`sr_game.ticks`).
3. Fill the fields of the type.
4. Move `sr_numofevents` forward by one. It wraps at 0x10000.

The server sends each client the events that the client did not confirm (`Connection.event_ack`). A vector is copied as raw bytes, so no float load changes its bits. A null vector gives a zero vector.

### Event types made by this code

| Type | Function | `param` | `attach_type`, `attach_id`, `range_mode` |
| --- | --- | --- | --- |
| 0 | `createevent_bullet` | bullet type | the weapon item |
| 1 | `createevent_bullethit` | the target | what the bullet hits (0, 1 or 2) |
| 4 | `createevent_updateobject` | object index | state, value |
| 5 | `createevent_updateitem` | `(active + type * 2) * 0x400 + item` | parent human, parent item, parent slot |
| 6 | `createevent_updateiteminfo` | item index | call state, info, one more item field |
| 7 | `createevent_updateplayer` | packed bot, active, team and player index | human, money, packed looks. The name goes in `message`. |
| 8 | `createevent_updateplayer_finance` | player index | money, stocks |
| 9 | `createevent_sound` | sound id | `sound_a` and `sound_b` hold two sound parameters |
| 16 | `createevent_updateblock_quad` | `x * 0x10000 + area` | `z * 0x10000 + y`, block, face |

Notes:

- Item events return when the item index is 0x400 or more.
- The player name is copied as 31 characters. The 32nd byte is 0.
- Type 16 packs each pair of numbers in one int: x with the area and z with y.

## The server tick

Each tick the main loop calls these functions in this order:

1. `servercycle`: the heartbeat to the master server (every 1024 ticks), the bandwidth share of each connection, and the credit.
2. `serverrecv`: count the players, drop idle connections, handle each packet that came.
3. The game simulation.
4. `serversend`: send a packet to each connection whose credit is below 1.
5. `eventsimulation`: handle the events that the tick made (the doors).

### Connection record

A connection (`Connection`, 0xf170 bytes, array `sr_connections`, count `sr_numofconnections`) is one client.

| Field | Meaning |
| --- | --- |
| `ip`, `port` | The address of the client, in host order. |
| `game_id` | The round number that the client echoes. -1 after a join. A client with another number gets the new game packet. |
| `is_admin` | Set by `/admin` with the right password. |
| `player_id` | The player of the client, or -1 when the player is gone. |
| `credit`, `rate` | The bandwidth. `servercycle` gives each connection `bandwidth / connections` as `rate`, and lowers `credit` by `rate / 60` each tick. `serversend` adds the bytes that it sends. A connection sends only when `credit` is below 1. |
| `idle_ticks` | Counts up each tick. A packet from the client sets it to 0. At 0x4b0 the connection drops. |
| `event_ack` | The first event that the client did not confirm. |
| `sequence` | Counts the object packets. It picks which items go out. |
| `voice[8]`, `voice_key` | The speakers that the client hears. `voice_key` is the field before slot 0. |
| `spectate` | The human that a dead player watches, or -1. |
| `obj_priority[]`, `human_priority[]` | The scores of the objects and humans (see Priorities). |
| `camera_pos`, `listener_pos` | The camera of the client, and the place of its head. |

## Receive

`serverrecv` runs these steps:

1. Count the players that are active and not bots (`server.num_players`).
2. Add 1 to `idle_ticks` of each connection.
3. Drop the connections that are idle for 0x4b0 ticks, that have no player, that have no account, or whose account has a ban time above 0. A dropped player gets `deleteplayer` and the message "name Exited". The last connection moves into the free place.
4. Read each datagram. A datagram from an address in the kick list (`kickedip`) is ignored. The first byte after the socket id is the type.

| Type | Meaning | What the server does |
| --- | --- | --- |
| 0 | Query | Reply with the version, the game type, the player counts, the name and (for version 7 or newer) the account key. |
| 2 | Join | Check the version and the free places. Make a player, read the name and the account id, find or make the account. A player of the same account that is still connected is replaced. A banned account is refused. The humans of the account go back to the player. |
| 4 | Input | Find the connection of the sender. Ignore the packet when `game_id` is not the round number. Read the axes, buttons, mode, camera, the actions and the 6 voice frames. |
| 7 | Leave | Set `idle_ticks` to 3000, so the connection drops on the next tick. |

A join sends no reply. The client waits for the new game packet (type 6) that `serversend` sends to a connection with another `game_id`.

### Input packet

The floats and the camera are copied as raw bytes. The actions go into a ring of 64 records in the player (`PlayerAction`): a menu choice (kind 0), a chat text (kind 1) or an item action (kind 2). `num_actions` counts the next record. The server masks the eight-bit start index to six bits, even with zero actions.

The voice frames go into the ring of the player (`Speex.voice_ring[player]`). Each frame has an index (8 bits), a size (8 bits), a mode (2 bits) and the bytes. The server masks the index to six bits immediately after its read. The proxy guard (`hook/netguard.c`) stays unchanged.

Input with a negative connection `player_id` is ignored. At 256 connections, a new account cannot join. A connected account reuses its current slot. Below capacity, account takeover keeps the same connection sequence.

## Send

`serversend` writes up to 2 packets for each connection with a credit below 1.

- **Type 6, new game.** For a connection with another `game_id`: the round number, the game type, the round mode and two start angles.
- **Type 5, state.** The ticks, the state of the game (the ready bits when the state is 1), the timer, the seed of the world, the team sizes, the player and its human, the look angles, the velocity of the human, the menu (and the shop rows for menu 10), the money, the HUD numbers, the inventory, the object packet and the events.
- **Type 8, voice.** The 8 voice slots. For each active slot: the human, the item and the last 4 frames of the speaker.

Each packet adds its size to `credit`. The number of events is written twice: first with the room that the packet has, and again with the number that fit, at the same place.

### Events in the state packet

The client gets the events from `event_ack` to `sr_numofevents`, at most 31. `packevent` writes one event:

- An old message (more than 600 ticks) or sound becomes a placeholder (type 0x12, 6 bits).
- A message of kind 4 goes only to an admin.
- A user (not an admin) gets a message of a human or an item only when it is near. The value is `256 / d`, where `d` is the distance to the listener. Range mode 0 multiplies it by 0.25, mode 2 by 2, and an item by 0.25 more. Below 4 the event is a placeholder.
- The player of a human gets its own chat text, the one before the word filter.

The fields of each type are in [protocol.md](../protocol.md). Types 8 and 11 to 16 write whole 4-byte fields.

## Priorities

`packobjectpacket` chooses what the player sees. Each connection has a score for each human and each object. Each tick the score grows by an amount from 1 (far) to 256 (at the same place), and by 0x400 for the own human and for objects within 4 units (12 for a vehicle of type 12). The amount is `(1 - (d ^ 1.5) / 256) * 256`, where `d` is the distance (twice as long for a human in a vehicle).

1. The best 8 humans go out in full, and their score returns to 0.
2. The best 2 objects go out in mode 0 (full), the next 6 in mode 1 (a turn of 21 bits) and the next 8 in mode 2 (only the place). Modes 0 and 1 reset the score.
3. An item is a candidate when it has no parent item and is not in a slot above 1 of a human. Let `x` be the packet counter `sequence` xor the item number. When the low 2 bits of `x` are not 0, the item must be within 32 units. When the low 5 bits of `x` are not 0, the item must be within 256 units, and an item that no human holds must move or turn faster than 1/120.
4. All city objects that are in use go out.
5. A client that is more than 23 events behind gets no humans, objects or items.

The sorting is `csradixsort` on the keys of the `Radix` record. `packobjectpacket2` is an older version (power 1.75, one list for humans and objects, a limit of 31 events). No code calls it.

## Voice

`calculateplayervoice` chooses the 8 voice slots of a connection when the option `voicechat` is on.

1. Slots in use: drop the speaker that left, that muted itself (`voice_off`), that is more than 256 units away, or whose radio link broke.
2. Players near: a player within 256 units with a free slot, or a slot that holds a farther speaker. The player that has no human is heard at distance 8.
3. Radios: an item of type 0x17 that is linked to another radio, held in slot 0 by a human, in state 3. The distance counts 4 times.

`addvoice` finds the free slot. When all 8 are busy it can replace only slot 0, and only when the new speaker is nearer than that one.

## Chat and admin commands

`server_playermessage` runs for each chat text. The word filter lowers the text, finds the filtered word and replaces the text with "i'm an idiot" (the real text goes to `orig_text`, only the player sees it). A text that starts with `/` is a command. A muted player (`chat_block` above 0) gets no chat.

`/admin PASSWORD` makes the player an admin. After 5 tries it stops. An admin can use:

| Command | Effect |
| --- | --- |
| `/hlist`, `/list1`, `/list2`, `/list3` | List humans (first 10) and connections (0 to 9, 10 to 19, 20 and up). |
| `/godmode` | Toggle `godmode` of the player. |
| `/heli` | Make a helicopter 4 units from the human, along the third row of its first bone. |
| `/phone` | Remove the phones of the admins and give the player a new one. |
| `/arm` | Arm the human (`scenario_armhuman`). |
| `/cash` | Add 100000 to the money. |
| `/resetgame` | Ask for a new round. |
| `/ban1 N`, `/bany T`, `/bann` | Choose the account N, ban it for T, or clear the choice. Without a number, `/bany` uses the length of the text minus 5 as the time. |
| `/give N M` | Add M to the account N and to its players. |
| `/kick NAME` | Drop the player and add the ip to the kick list. The list has no size limit. |
| `/message TEXT` | Send a message to everybody. |
| `/clearkick` | Empty the kick list. |

The prefix tests are plain `strncmp`, so `/army` also runs `/arm`.

## The master server

`servercycle` sends a heartbeat packet (type 0x20) every 1024 ticks. `getmasterserverip` runs once at the start: it asks the page `www.crypticsea.com/anewzero/serverinfo.php` for the address of the master server and 8 lines of text. It waits up to 30 seconds for the send and 30 for the answer. When the page gives no address, the code uses 216.55.186.104:27590.

## The client side

These functions read what the server writes. The dedicated server never calls them. They exist for the client and are tested with made-up packets (`tools/lift/synth_net.py`).

- `clientrecv` handles type 6 (new game), type 5 (state) and type 8 (voice). A state packet is skipped when its tick is 1 to 99 behind the client tick. After 0x4b0 ticks without a packet it sets `game.link_state` to 3.
- `unpackobjectpacket` reads the four lists. It keeps the last two states of each human and object (`net_tick`, `net_pos`, `net_tick_prev`, `net_pos_prev`) to move the thing smoothly. For a human it rebuilds the bones from the joint angles and the body table.
- `unpackevent` reads one event. `unpackorientation`, `unpackangle`, `unpackscalar` and `pbitreadnego` read the packed numbers.
- After a state packet the client rebuilds the inventory lists of the humans from the items.

## Codecs

| Function | Purpose |
| --- | --- |
| `packscalar` | A number from -range to range as a sign bit and bits - 1 of size. |
| `packunitvector` | A unit vector as the axis of its largest part and two ratios. No code calls it. |
| `packpositiondelta`, `unpackpositiondelta` | The difference of two positions. No code calls them. |
| `unpackangle` | The reverse of `packangle`. |
| `unpackorientation` | The reverse of `packorientation`. |
| `pbitreadnego` | Read bits where all bits set mean -1. |
| `cs_pread` | Read whole bytes at the next byte. The bounds check includes byte alignment. |

## Short packets

`serverrecv` checks the full join or input packet before either handler changes state. The check includes the voice flag and spectate field. A packet without a type byte is ignored. Readers include partial bits and byte alignment in their bounds checks. A short read stops all later reads until the next packet. A negative receive size means no packet and keeps the cursor unchanged.

The invalid input mode uses sanitized packets or state checks. S1 cases cover both full-pool paths, short joins, input trailers, and failed reads.

## Functions

| Function | RVA | File | Purpose |
| --- | --- | --- | --- |
| `pbitwrite` | 0xda60 | packet.c | Write bits to the send buffer. |
| `pbitread` | 0xdb20 | packet.c | Read bits from the receive buffer. |
| `pbitwritenego` | 0xddb0 | packet.c | Write bits and return 1. |
| `cs_pwrite` | 0xd990 | packet.c | Write whole bytes at the next byte. |
| `recvpacket` | 0xd860 | packet.c | Receive one packet and check the socket id. |
| `sendpacket` | 0xd910 | packet.c | Send the send buffer to an address. |
| `packangle` | 0x16680 | packet.c | Write an angle in a set number of bits. |
| `packposition` | 0x16790 | packet.c | Write a position as three floats. |
| `packorientation` | 0x15df0 | packet.c | Write a unit vector as an octahedron path. |
| `cs_pread` | 0xd9f0 | packet.c | Read whole bytes at the next byte. |
| `pbitreadnego` | 0xddd0 | packet.c | Read bits where all bits set mean -1. |
| `packscalar` | 0x16860 | packet.c | Write a number as a sign bit and a size. |
| `packunitvector` | 0x5160 | packet.c | Write a unit vector as an axis and two ratios. |
| `packpositiondelta`, `unpackpositiondelta` | 0x2efd0, 0x2f090 | packet.c | The difference of two positions. |
| `unpackangle` | 0x16710 | packet.c | Read an angle. |
| `unpackorientation` | 0x16320 | packet.c | Read a unit vector from an octahedron path. |
| `createevent_updatehuman` | | events.c | Event 17: a human takes a player. |
| `createevent_updatestocks` | 0x6840 | events.c | Event 13: the prices of the 5 stocks. |
| `createevent_updatecarmarket` | 0x68d0 | events.c | Event 14: a row of the car market. |
| `eventsimulation` | 0x61a0 | events.c | Handle the door events. |
| `serverrecv` | 0x33650 | server.c | Receive and handle the datagrams. |
| `servercycle` | 0x33570 | server.c | Heartbeat, bandwidth and credit. |
| `disablesocket` | 0xd710 | server.c | Open a UDP socket. |
| `serversend` | 0x4ed30 | server.c | Send the new game, state and voice packets. |
| `addvoice` | 0xb6d0 | server.c | Find a voice slot. |
| `calculateplayervoice` | 0x3cfe0 | server.c | Choose the voice slots. |
| `packevent` | 0x16f10 | netpack.c | Write one event. |
| `packobjectpacket` | 0x20680 | netpack.c | Choose and write the objects. |
| `packobjectpacket2` | 0x1f4a0 | netpack.c | The older version. No caller. |
| `server_playermessage` | 0x49940 | commands.c | The chat filter and the admin commands. |
| `getmasterserverip` | 0x1ac40 | master.c | Ask the master server for its address. |
| `clientrecv` | 0x2e310 | client.c | The client handles the packets. |
| `unpackevent` | 0x17b60 | client.c | Read one event. |
| `unpackobjectpacket` | 0x1dba0 | client.c | Read the object lists. |
| `createevent_updateitem` | 0x6380 | events.c | Event 5: the place and state of an item. |
| `createevent_updateiteminfo` | 0x6420 | events.c | Event 6: the call state and info of an item. |
| `createevent_updateplayer` | 0x64a0 | events.c | Event 7: a player. |
| `createevent_updateplayer_finance` | 0x65f0 | events.c | Event 8: money and stocks of a player. |
| `createevent_bullet` | 0x16bb0 | events.c | Event 0: a bullet leaves a weapon. |
| `createevent_bullethit` | 0x16c40 | events.c | Event 1: a bullet hits something. |
| `createevent_updateobject` | 0x16ce0 | events.c | Event 4: the state of an object. |
| `createevent_sound` | 0x16db0 | events.c | Event 9: a sound at a position. |
| `createevent_updateblock_quad` | 0x16e30 | events.c | Event 16: a face of a block breaks. |

## Known odd behavior

- `addvoice` also reads the field before slot 0 (`voice_key`). The slot test for a full list compares only with slot 0.
- A NaN distance keeps a speaker that already has a slot (`256 < NaN` is false) but never adds a new speaker.

- `recvpacket` compares the first byte of the socket id as a signed char. An id byte of 0x80 or more never matches.
- `pbitread` does not count the leftover bits of the last byte. It can read up to 7 bits past the end of a packet.
- `packorientation` tests each side with a compare that is false for a NaN. The form of each test decides which branch a NaN takes.
