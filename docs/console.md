# Bots and the operator console

Status on 2026-11-28: a first version works. It is a side project of the recording work, set aside on purpose. This page says what exists, what is wrong, and what to do when the work starts again.

## What exists

| Part | File | What it does |
| --- | --- | --- |
| Bot player | `instrumentation/recording/bot.py` | Joins the original server like a client (UDP, `tools/net/srpacket.py`). Picks a team in a round, or moves its human into the safe box of a base by memory write (World mode). Walks the menus of the base, visits shops and banks, asks the recorder for a snapshot, leaves, joins again. |
| Scenarios | `instrumentation/recording/scenarios.txt` | `#bot` lines start bots after the original server (launcher: `StartScenarioBots`). Scenarios: `fast-rounds`, `company-missions`, `accounts`. |
| Live view | `instrumentation/recording/live_view.py` | Starts the test server (`build/testserver`) and the console in one command. |
| Console | `tools/net/mainframe.py` | Operator console: players (name, slot, account id), teams (manager, members, missions), SIMULATION, call counters of the recorder, operator log, key bar in the style of the DOS file managers. Reduced color (green) or full color (names in the colors of the companies). |
| View | `tools/net/viz.py`, `tools/net/vector.py` | 3D wireframe of the world: map buildings and roads from `data/city.sbc` and the `.sbb` files, the safe zone of each company, humans from the bone tree of the game (15 bones, the pose of the packet), cars, items. Four cameras (follow, orbit, free, map). Output: braille dots, half blocks, sixel. |
| Output | `tools/net/screen.py` | Writes only the cells that changed, inside a synchronized update. |

Test with `python -I tools/net/test_viz.py`, `test_mainframe.py`, `test_screen.py`, `test_srpacket.py`, and without `-I` `test_vector.py` and `test_mainframe.py` (numpy). The gate has three bot recordings (`build/recordings/20261128-bot-*`) and 253 hand written calls (`instrumentation/recording/synth_calls.txt`). See `docs/rerecord.md` for the keys and the arguments.

## Known problems

1. **Speed.** The picture runs at 5 to 10 pictures each second, and the line DISPLAY of the SIMULATION panel shows it. The pipeline is not optimized: the scene is rebuilt in Python each picture, each line is rasterized by a loop, the console panels are formatted again each picture, and the render runs in the same thread as the network loop of the bot. Sixel is slower still (about 330 ms for a picture).
2. **The operator log does not hold.** An entry appears and goes away. The log shows the events of the last packet, and a packet holds few of them. The log needs its own buffer that keeps the lines, with a scroll.
3. **Camera in a building.** The building that holds the camera is not drawn. The view then shows the inside without walls.
4. **Detail.** At the default distance a human of 15 bones is a few dots in braille. Cars are two boxes and wheels, and buildings are boxes with floors and window posts.
5. **Timing tests.** The frame time tests of `test_mainframe.py` fail now and then on a busy machine. They scale the limit with a short calibration loop.

## Plan

1. **A new pipeline.**
   - Split the work: a thread reads the network and keeps the last state, a thread draws at a fixed rate and drops pictures that are late.
   - Cache the static layers (map, grid, compass) as projected segments and move them with the camera only.
   - Build the lines of all humans, cars and items in one numpy batch.
   - Redraw only the changed cells of the view (dirty rectangles), not the picture.
   - Measure before and after with the DISPLAY line. The target is 30 pictures each second at 150 by 46 cells in braille.
2. **Settings inside the program.** A menu (key F1 or a settings key) to change: render mode, color, camera defaults (distance, angle), labels, the log filter, the target. Save the choices in a small file. Today only flags and keys change them.
3. **The log.** A real buffer with timestamps, severity filter (I, W, E), scroll, and a mark for the events of one player.
4. **More features.** Replay of a recorded packet file with play, pause and speed. A spectator that follows any human (done with TAB and the brackets) and a list to pick one. An admin panel for the new server (its own API, not memory writes). A map viewer for the level files.
5. **Models of the game.** Cars from the object types (faces and particles read from the memory of the server, the object id gives the type). Buildings from the floor plan of the `.sbb` files (cells and blocks), not from a box.
6. **The browser.** If the terminal stays the limit, draw in a browser (WebGL) and send only the scene data over a websocket. The terminal can only be as fast as its own pixels and the characters it takes each second. The browser gives true pixels, mouse control and no flicker. The console panels would become HTML.

## Notes

- The game data of the map (`data/`) may be versioned. See `CLAUDE.md`.
- The bot works only with the original server of the recorder (fixed image base). Never run it on a server with real players.
- The player name and the account id come from the memory of the original server. The new server needs its own API for them.
