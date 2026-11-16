# Events and the game tick

Files: `src/net/events.c`, `src/include/sr_runtime.h`.

The server writes events into a ring (`event`, 128 bytes each). `eventsimulation` reads them. The client uses the same records and the same functions.

## Shared functions

`createevent_bullethit`, `createevent_updateobject`, `createevent_updateitem`, `createevent_updateiteminfo`, `createevent_updateplayer`, `createevent_updateplayer_finance`, `createevent_updateblock_quad` and `eventsimulation`. Each gives the same result as the client on its synthetic calls. `createevent_updateobject` skips 10 of 90 calls that the test cannot make.

## The game tick

An event stores the tick of the game. The client keeps the tick in `Game+0x34` (`game_tick`). The server keeps it in `Game.ticks` (+0x38). In the client, `Game+0x38` holds the time of SDL (`last_tick_ms`).

Shared code reads the tick through `sr_game_ticks`. The client adapter points the macro to `game_tick`. The server build reads `Game.ticks`. See [records](records.md).

## Packets

The bit functions of `src/net/packet.c` are shared for writing: `pbitwrite`, `pbitwritenego` and `cs_pwrite`. The read functions `pbitread` and `pbitreadnego` are separate in the client. The client reads 32 bits at any bit position. The server version loses bits there. A read changes the protocol, so the client keeps its own version.

## Not shared yet

`createevent_sound` has another signature in the client (3 parameters, the server has 4). The name needs a check before the function is shared.
