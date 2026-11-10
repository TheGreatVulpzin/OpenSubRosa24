# The tick of the client

Files: `src/client/setup/simulation.c`, `simulation.h`.

`checksimulation(allow_input)` runs from the main loop. The result is 1 when a tick ran and 0 when the time of the next tick has not come.

## Time

- A tick is 16 ms. With `slow_tick` not 0 it is 100 ms.
- The last tick time is `last_tick_ms`. A tick that comes 250 ms or more late starts from now. A tick that comes less than 250 ms late moves the last tick time by one interval, so the ticks catch up.
- Each tick writes the milliseconds it took into a ring of 32 values (`frame_times`) and the mean of the ring (`frame_time_average`).

## Input

When `allow_input` is not 0 the function reads the input first.

- `getinputs` for the viewed player (`view_player`, -1 for none). With no player in view, `movecamera(0)` moves the free camera.
- A key counts when it is down in `keyboard` and was not down in `prevkeyboard`.
- F1 toggles `voice_on`. With the debug flag on, the = key does it. F4 toggles `hide_hud`.
- Page Up and Page Down change `mic_gain_percent` by 10, from 30 to 250, and set `mic_text_timer` to 4 seconds. The gain `audio_voice_gain` is the percent divided by 100, from 0.125 to 4.
- A host restarts the game with F5.

## The tick

The tick does not run while `paused` is not 0. `step_once` makes one tick run, and then the game pauses again.

A client sends its input and receives the state of the server (`clientsend`, `clientrecv`). The viewed human takes the aim of its player: on foot the body and the arm, in a vehicle the vehicle aim. Then the human and item smoothing run. When the round number of the server changes, the game restarts.

A host runs the server tick: the bots (only in state 2), `servercycle`, `serverrecv`, `playersimulation`, the physics pass, `logicsimulation` and `serversend`. With the debug flag on, the V key sets the health of human 1 to 100.

Both then count the tick, and in the states 0, 1 and 2 they run the bullets, the events, the sprites, the tire marks and the time to live of bullets and sprites. A client also sets the traffic lights: every intersection has two sides that take turns by the low 11 bits of `host_ticks`.

| Ticks in the cycle | Sides 0 and 2 | Sides 1 and 3 |
| --- | --- | --- |
| 0 to 0x37f | red | green |
| 0x380 to 0x3ff | red | yellow |
| 0x400 to 0x77f | green | red |
| 0x780 to 0x7ff | yellow | red |

## Display values

- `damage_flash` becomes 1 when the health of the viewed human drops, then falls by 1/16 each tick.
- `death_fade` grows by 1/240 each tick while the viewed human is dead and the state is 2. It stops at 1.
- `mic_text_timer` falls by 1/60 each tick.
- Every chat message timer falls by 1 each tick, down to 0.

The light angles follow the clock in gametype 4. With the debug flag on, [ and ] turn the first angle and Shift with them turns the second.
