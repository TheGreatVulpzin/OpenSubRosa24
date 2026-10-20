# Network

The network code has two parts. The packet code reads and writes the bit stream of one UDP packet. The event code makes the server events that the server sends to clients. Files: `src/net/packet.c` and `src/net/events.c`. The wire format is in [protocol.md](../protocol.md). This page does not repeat it.

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

- `recvpacket` compares the first byte of the socket id as a signed char. An id byte of 0x80 or more never matches.
- `pbitread` does not count the leftover bits of the last byte. It can read up to 7 bits past the end of a packet.
- `packorientation` tests each side with a compare that is false for a NaN. The form of each test decides which branch a NaN takes.
