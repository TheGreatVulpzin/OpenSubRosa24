# What the client sends and how it smooths the world

Files: `src/client/setup/send.c`, `src/client/setup/interpolation.c`, `src/client/setup/server_list.c`.

`getonlineserverlist` asks the master server for the list of online servers. It sends one packet of type 0x21 with no other data to the address and port of the master server. It sends nothing while the address is zero.

## The input packet

`clientsend` sends one packet of type 4 each tick when a player is in view (`view_player` is not -1). `serverrecv` reads it in the same order. The packet has these parts:

| Part | Size | Content |
| --- | --- | --- |
| Header | 4 bytes | Written by `resetsendpacket`. |
| Type | 1 byte | 4. |
| Round | 4 bytes | The round number of the game. |
| Inputs | 9 floats | The four analog axes, the yaw, the pitch, two extra aim values and the button bits of the player. |
| Modes | 8 and 4 bits | The input mode and the move mode. |
| Last event | 16 bits | The low 16 bits of `last_event`. |
| View place | 3 floats | The place of the camera. |
| Actions | 4 and 8 bits, then the actions | The number of queued actions, at most 15, and the index of the first one. |
| Voice | 6 frames | For each frame: its index, size, mode and bytes. |
| Voice off | 1 bit | The flag of a muted speaker. |
| Watched human | 8 bits, signed | `spectated_human`. |

The packet goes to `server_address` and `server_port`.

An action is one of three kinds. A menu action has a menu id (8 bits) and a value (4 bytes). A chat action has a length (6 bits), the text bytes and a mode (4 bits). An item action has two 16 bit numbers. The actions are in a ring of 64 in the player record. The count is the distance from the tail to the head of the ring.

The 6 voice frames are the 6 frames before `head` in the ring of the viewed player. Their indexes wrap around 64.

## Smoothing

The server sends the places of humans and objects a few times each second. The client keeps the last two places and the tick of each. Every tick:

- `client_simulation` puts a human or an object along the line between the two places. The point goes as many steps past the newest place as ticks passed since it came, and never more than 64. The bones of the human hang from their parents by the joint offsets, turned by the joint angles.
- `humansimulationclient` copies the place of the first bone to a human on foot. A human in a vehicle sits on its seat: the seat place is in the frame of the vehicle. While the player holds the exit button, the human moves out along the first row of the frame, one step for each tick up to 30. The arms and the legs then take the frame of their parents. Two of them (bones 9 and 12) are turned by a quarter of a turn.
- `itemsimulationclient` counts down the timer of each item. An item that hangs from another item takes its place, and items of type 15 and 16 on a holder of type 13 or 14 move along its first row by the slot number. Other items go down by 1/16 of the second row. An item that a human holds takes the frame of the hand bone and a place 1/4 behind it.

## The objects of a state packet

`src/client/setup/unpack_objects.c` (`cl_unpackobjectpacket`, the client version of `unpackobjectpacket`) reads the humans, the vehicles, the items and the elevators of a state packet, then runs `client_simulation`. The server version in `src/net/client.c` gives other results: the client makes row 0 of an orientation with `crossproduct` of rows 2 and 1, and it places each bone with `scaleaddvectors` and three turns. The bit widths of the fields have names in the file.

Test: `synth_calls.py unpackobjectpacket` reads the bits that come next in the snapshot. On one snapshot both calls are the same. On the other two the bits run past the end of the 64 KB buffer, and the shared `pbitread` stops there (`recv_pos` becomes 0x10000) where the client did not: one byte of `recv_pos` differs. A real packet does not run past the end.

`getlanipaddress` (in `front.c`) adds each address of this machine (at most 16) with the last byte 255 and the game port to the join list, before the master list.
