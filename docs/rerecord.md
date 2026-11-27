# Recordings to make again

The recordings of `build/recordings` are not in git. Nine of them were lost on 2026-11-25. This page says what each one covered and how to record it again. The gate (`gate.py`) lists the functions of a lost recording as `DROP` until a new recording reaches them.

Never commit a recording. It holds player names, IP addresses and `config.txt`.

## The launcher and the checklist

The launcher (`build\launcher\opensubrosa24_launcher.exe`, build it with `tools\launcher\build.bat`) records the server and the client together. Turn on Debug Mode, choose the preset Harness Recording and open the tab Recording.

1. Client folder: the folder of the client with `subrosa_fixed.exe` (`build\hook\subrosa_fixed.exe`, made by `tools/re/fixed_base.py`). The launcher puts the recording `wsock32.dll` (`client_wsock32.dll` next to the launcher, or `build\hook\client\wsock32.dll`) and a `subvulpes.ini` there. Only the original client code runs.
2. Tick "Start the client with the server" and click Start. The server starts on the port of its settings, the client 6 seconds later. In the client join `127.0.0.1`.
3. The list in the tab shows each function of the server or the client with its recorded calls and a short text. A function is ticked at 2 calls. The list reads the recordings of all past tests and the running one, and updates each 15 seconds. "Missing only" hides the ticked ones. The text comes from `instrumentation/recording/hints.csv`, else from the comment above the function in the code.
4. Before each start the launcher writes `record_focus.txt`: the functions that are still missing stay hooked all the time.
5. Stop the server and close the client. The server files stay in `logs\<test>`, the client files go to `logs\<test>\client`.

`python -I instrumentation/recording/coverage.py --side server|client` makes the same checklist as a file (`build\recordings\CHECKLIST-<side>.md`) from the recordings in `build\recordings`. After a change of the functions or the hints run `python -I instrumentation/recording/coverage.py --export instrumentation/recording/functions.csv`: the launcher reads that list (the package has it as `recording_functions.csv`).

The server snapshot needs 600 seconds of play, the client snapshot 240 seconds (`snapshot_after` in `launcher.ini` sets the server value).

## Server scenarios (the forcer)

A short test does not reach every state of the game. The forcer writes chosen values into the game memory at chosen ticks, on the game thread, between two calls (`instrumentation/hook/force.c`). The original server records the result like any other play.

The launcher lists the scenarios in the tab Recording ("Server scenario"). The choice goes to the server folder as `subvulpes_force.txt` when the original server starts, and the file moves to `logs\<test>` with the recording. With "None" there is no file and the game is not changed.

| Scenario | Mode | Effect |
| --- | --- | --- |
| `fast-rounds` | Round | A round ends in about 4 minutes. The server goes through states 1, 2 and 3 and `resetgame` many times. |
| `missions-now` | Round | The missions, traffic and team cars appear as soon as state 1 starts. The round keeps its length. |
| `world-clock` | World | The world clock jumps 30 ticks before each period of 54000 units (shops, bans, missions) and before the end of the world. |

Read a scenario with the field names: `python -I instrumentation/recording/force.py show instrumentation/recording/scenarios.txt fast-rounds`. `force.py find game.timer` gives the RVA and the size for a new rule. A new scenario is a new `[name]` section in `instrumentation/recording/scenarios.txt`. The rules are numbers, so the recorder needs no table of names. The format is at the top of `force.c`, and `instrumentation/tests/test_force.c` tests it.

Limits:

- The forcer writes memory. It cannot call a function, so it cannot choose the kind of a mission. A player can use the admin chat commands (`/resetgame`, `/cash`, `/arm`, `/give`) for more.
- A write after the snapshot is not in the snapshot. The harness compares the original code with the clean code on the same state, so this gives no false difference. A state that only a late write creates may not replay as it ran. Put the writes that must be in the snapshot before `snapshot_after`.
- The game sets the tick counter back to 0 at each `resetgame`. A rule with `every` starts again from the first tick of the new round. A rule with `at` fires one time.
- The forcer was tested only on a fake image (`test_force.exe`). The first run on the original server is the real test: check `subvulpes.log` for the lines `force: tick ... wrote`.

The sections below say what the old recordings held and what the player does.

## Server (024d)

Surviving: `play-20261012`, `20261013-1016-smoke`, `20261013-query`, `20261015-friend`, `murilo-20261015` (with its synthetic runs). The gate on these shows 0 differences.

Functions that no surviving recording reaches (13). A new recording must do the action in the right column:

| Function | What the player does |
| --- | --- |
| `logic_gunstore_buy` | Buy a gun at the gun store |
| `item_addbill`, `item_removebill`, `item_billvalue` | Pick up, spend and drop bills (money items) |
| `createevent_updateplayer_finance` | Any change of a player's money |
| `saveaccountsserver`, `server_writelogfile` | Let the server save the accounts and write the log (play a few minutes, then stop the server in the normal way) |
| `logicsimulation_round` | One full round: the round ends and the next one starts |
| `player_loadinventory`, `player_saveinventory` | A player leaves in a round and joins in the next one (the server saves and loads the inventory) |
| `sphereintersectarea`, `sphereintersectcityobjecttype` | Collision tests of a sphere with a building and a city object (move objects and cars in a building and near city objects) |
| `multiply3x3matrices` | Not known. The old recording reached it twice. Play with cars, objects and several players |

Functions that fell to one or two calls (33). They still pass, but with little proof. A longer session with these actions reaches most of them:

- Teams: `logic_addteamcash`, `playerteamkilltax`, `createmission_threeway`, `createmission_transaction`, `scenario_getnumofmissions`
- New game and level: `resetgame`, `createlevel`, `createscenario_world`, `createplayer`, `createcityobject`, `city_createelevator`
- Cars and items: `logic_createcar`, `linkitemtoitem`, `objectwheelsimulation2`, `item_sphereintersect`, `playerbuystock`, `createevent_updatecarmarket`
- Shots: `createbullet`, `bullettimetolive`, `lineintersecthuman`, `lineintersectcityobject`
- Network: `pbitread`, `packposition`, `sendpacket`, `servercycle`, `account_nameexists`

Plan for one session of about 40 minutes with 2 or 3 people (the second person joins late, so the inventory path runs):

1. Start the launcher preset `Harness Recording` (`SV_RECORD_DIR`, snapshot after 600 s by default).
2. Everybody buys a gun, picks up bills, drives a car, shoots, kills a player of another team (team kill tax) and spends money at a store.
3. Let one round end. A player leaves before the end and joins in the next round.
4. Stop the server in the normal way, so the accounts and the log are saved.
5. Run `python -I instrumentation/lift/min_calls.py --keep 2`, then `python -I instrumentation/lift/gate.py --update` (ask first: it replaces the baseline).

## Client (0.24b)

All three client recordings are lost, so the client gate has no data. A recording needs the proxy `wsock32.dll` of the target `cl_wsock32` and the fixed copy `subrosa_fixed.exe` (see `docs/client.md`, "Recorder" and "Harness for the client").

| Lost recording | What it held |
| --- | --- |
| `client-menu-20261103` | 50 s on the main menu, snapshot after 20 s |
| `client-play-20261103-1229` | 14 min of play, snapshot after 180 s |
| `client-play2-20261103-2019` | 25 min of play, snapshot after 240 s, with a hosted game and the two demos |

Record these sessions again, with `record_args=1`, `record_imports=1` and `hooks=1` in `subvulpes.ini`:

1. Main menu: open every menu (options, video modes, controls, voice options, credits), then leave. Snapshot after 20 s.
2. Join a server: walk, shoot, drive, buy, use a phone, open the map, the inventory, the company list (key C) and the chat. Snapshot after about 3 minutes, and keep playing for 10 minutes.
3. Host a game on the same PC and play 10 minutes (the server paths of the client).
4. Demo 1 and Demo 2.
5. The editors: building, level and model editor, each for a few minutes, and save one.
6. Take a screenshot, change the video mode, and try the voice test.

The functions that no earlier recording reached stay with the synthetic tests (`instrumentation/client/synth_*.py`). Those scripts use the snapshot of `client-play2-20261103-2019`. Make `sv_snapshot.bin` of the new play recording before you run them again.

After the recording: run `python -I instrumentation/client/cap_calls.py build/recordings/<recording>` (2 calls per function) and add the names to `instrumentation/lift/targets/client_win.json`.
