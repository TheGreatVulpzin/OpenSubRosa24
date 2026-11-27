# Records of the client

Shared code reads `sr_game.<field>` and `sr_option.<field>`. The client record has the same field names as the server record. Some fields sit at other offsets. The client adapter uses its own types for them.

## Game record (`GameClient`)

The client keeps the game record at RVA 0x9065300. It has 0x25d60 bytes. The server record `Game` has 0x25d64 bytes.

| Area (server offset) | Client offset |
| --- | --- |
| +0x00 to +0x33 | the same |
| +0x38 to +0x57c3 (tick, debug switches, sun angles, five team records) | 4 bytes before the server field |
| +0x57c4 to +0x257e3 | the same |
| +0x257e4 to the end (checkpoints, count of newspapers) | 4 bytes before the server field |

- The client has no `end_time`. A team record has 0x117c bytes in both records.
- `GameClient` in `src/include/sr_types.h` is written by hand. A new field of `Game` needs its own place in `GameClient`, at the client offset.
- The game tick sits in `Game+0x34` in the client. The shared code reads `sr_game_ticks`. In the server it is `Game.ticks`.
- `client_account_key` (client `Game+0x25d48`): the host menu copies the key of the account store here. The join menu copies the key of the chosen server.

## Option record (`OptionClient`)

The client keeps the option record at RVA 0x90630e0. It has 0x150 bytes.

- The client has no `gametype` (server +0x118). Every field after it sits 4 bytes before the server field.
- `roundtime` +0x118, `setcpuaffinity` +0x11c, `occlusion` +0x120, `voicemin` +0x124. `help` +0x138, `voiceboost` +0x140, `voiceboost_scale` +0x144. `maxnumofplayers` (+0x114) is the same.
- Shared code reads the game type through `sr_option_gametype(record)`. In the client it is a private variable.
- `OptionClient` in `src/include/sr_types.h` is written by hand, at the client offsets.

## Macros of the adapter

| Macro | Server | Client |
| --- | --- | --- |
| `sr_game_ticks` | `Game.ticks` | `game_tick` (`Game+0x34`) |
| `sr_game_end_time` | `Game.end_time` | a private variable |
| `sr_option_gametype(o)` | `o->gametype` | a private variable |
| `sr_parser_file` | `Parser.file` | the global `open_file` (RVA 0x13e61828) |
| `sr_debug_cross(c, size)` | not defined | `debug_addcross` |
| `sr_debug_float(name, value)` | not defined | `debug_addatafloat` |
| `sr_debug_line(from, to)` | not defined | `debug_addline` |

A debug macro is not defined in the server. The shared code wraps each call in `#ifdef`. The client shows these debug values: the target of the hand (`human_calcarmtorque`), and the speed and angle of the rotor (`object_rotorsimulation`).

## Data objects with a client address

These objects have a client address in `analysis/client_win/storage.csv`.

| Object | Client RVA |
| --- | --- |
| `bond` (16384 records of 0xec bytes) | 0x1779b820 |
| `bondopt` (16384 records of 0x40 bytes) | 0xf8ae240 |
| `bondparticle` (16384 records of 0x20 bytes) | 0x17718de0 |
| `random_counter` | 0x90f6080 |
| `carcolorname` (6 pointers) | 0x123568 |
| `bezier_curve`, `bezier_curve_uv`, `bezier_curve_dv` | 0x75a57c0, 0x13c5e6e0, 0xaff0520 |

A function that reads an object with no address cannot be shared. `python -I tools/re/check_adopted.py --file <list>` lists those functions.
