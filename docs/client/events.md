# Events and the game tick

Files: `src/net/events.c`, `src/include/sr_runtime.h`.

The server writes events into a ring (`event`, 128 bytes each). The client uses the same records. The client consumes them with `client_eventsimulation`. The server function `eventsimulation` handles garage doors only.

## Shared functions

`createevent_bullethit`, `createevent_updateobject`, `createevent_updateitem`, `createevent_updateiteminfo`, `createevent_updateplayer`, `createevent_updateplayer_finance`, `createevent_updateblock_quad`. Each gives the same result as the client on its synthetic calls. `createevent_updateobject` skips 10 of 90 calls that the test cannot make.

## The game tick

An event stores the tick of the game. The client keeps the tick in `Game+0x34` (`game_tick`). The server keeps it in `Game.ticks` (+0x38). In the client, `Game+0x38` holds the time of SDL (`last_tick_ms`).

Shared code reads the tick through `sr_game_ticks`. The client adapter points the macro to `game_tick`. The server build reads `Game.ticks`. See [records](records.md).

## Packets

The bit functions of `src/net/packet.c` are shared for writing: `pbitwrite`, `pbitwritenego` and `cs_pwrite`. The read functions `pbitread` and `pbitreadnego` are separate in the client. The client reads 32 bits at any bit position. The server version loses bits there. A read changes the protocol, so the client keeps its own version.

## Not shared yet

`createevent_sound` has another signature in the client (3 parameters, the server has 4). The name needs a check before the function is shared.

## Client consumer

`client_eventsimulation` consumes the event ring between `last_event` and `numofevents`.
The cursor wraps after slot 65535. An empty ring changes no state.

Each record supplies `type`, `tick`, two vectors, four integer words, sound values and message text.
The integer words change meaning with the event type.

| Type | Action |
| --- | --- |
| 0 | Add a bullet for a remote client, eject a casing, play a shot sound and set the muzzle timer. |
| 1 | Add a surface mark, debris, blood or sparks. |
| 2 | Add a display message or a newspaper line. |
| 3 | Activate an object and reset its face and tire state. |
| 4 | Change object health, faces or tires. A window hit also emits glass. |
| 5 | Decode item activation, type and parent links. |
| 6 | Set item call state, information and phone line. |
| 7 | Decode player state, appearance, money and human link. |
| 8 | Set player money and stocks. |
| 9 | Play a positional sound with the supplied volume and pitch. |
| 10 | Add or remove team garage doors. |
| 11 | Decode round state, three team states and one scenario record. |
| 12 | Decode mission state. An inactive mission retains its payload. |
| 13 | Set the five integer stock prices. |
| 15 | Set an interior block. |
| 16 | Break a special interior block window and mark its face pair. |
| 17 | Activate or remove a human and update the player link. |

Other types, including 14, only advance the cursor.
A host skips replicated object, item, player, human, scenario, mission, stock and block writes.
The host emits effects and applies garage door events.

Sound age uses unsigned tick subtraction. Ages below 100 play sound.
A future tick fails this test. Newspaper lines do not expire.
Glass events consume a random pitch value even when their sound expires.
Impact effects consume random values in mark, sound, velocity and size order.
Each size and pitch expression casts its result to float before the function call.
The vector functions round each result when they store the output vector.

Player updates set four color vectors and copy exactly 32 name bytes.
A player update copies team and suit flags to its human.
A human update copies colors and the other appearance values from its player.
Removing a human clears the player's human link but does not clear the human's player field.
`Mission.partner_detail` holds the high partner nibble of the event word.
The existing game field `unused_22fd8` counts scenario records on the client.
Its client offset is four bytes before its server offset.
