# System and main loop

This page covers the small systems that every server run uses: the text parser, the settings, the profiler timers, the file lists, the radix sort, the clock and the log files. It ends with the main loop of the dedicated server.

Files: `src/common/parser.c`, `files.c`, `util.c`, `system.c`, `random.c`, `src/game/options.c`, `profiler.c` and `src/server/main_loop.c`.

## Text parser

The parser reads a text file into one global buffer and moves a cursor over it. `config.txt` and `readme.txt` use it.

| Field of `Parser` | Use |
| --- | --- |
| `pos` | The cursor. |
| `size` | The number of bytes in the buffer. |
| `text` | The file bytes (1 MB) and a 0 after the last byte. |
| `number` | The digits that `getint` collects (256 bytes). |
| `file` | The `FILE *` of the open file. |

Functions:

| Function | RVA | Use |
| --- | --- | --- |
| `loadtextfile` | 0xd020 | Reads a file into `text`, sets `size` and sets `pos` to 0. |
| `checkstring` | 0xd0c0 | Returns 1 when the text at the cursor starts with the key. The cursor does not move. |
| `findstring` | 0x29650 | Moves the cursor to the first place where the key starts, then past the key. Returns 1 when it finds the key. |
| `getint` | 0xd110 | Skips to the next digit or minus sign, collects up to 255 of them and reads them with `sscanf`. |
| `getstring` | 0xd240 | Copies the rest of the line. |
| `cs_getline` | 0xd2a0 | Copies a line and moves the cursor to the start of the next line. |

Odd behavior:

- `loadtextfile` reserves the last buffer byte for zero. Oversized files give no text.
- When the file does not open, the buffer keeps its old text.
- When `findstring` does not find the key, the cursor goes to `size` plus the length of the key. The callers set the cursor to 0 after each key.
- `getint` returns 0 when the text has no number. The game code returns whatever was on the stack. The recorded calls are all of this kind, so the harness lists `getint` as a known difference.

## Settings

`loadsaveoptions` (0x4e7a0) sets the defaults and then reads `config.txt` from the settings folder. A non-zero argument only changes to the settings folder and back.

The defaults are the player name "Agent" and a number up to 511, the server name "Sub Rosa Server", a random server password, port 27584, 16 players, game type 3 and round time 7. The two random values use `srand` with the clock and `rand` of the game library.

Each key is a line `name=value`. The keys go in this order. Each key search starts at the top of the text.

- `Config` (the window of the client): `screenwidth`, `screenheight`, `colordepth`, `depthbits`, `stencilbits`, `fullscreen`, `joystick`, `sound`.
- `Option`: `soundvolume`, `musicvolume`, `port`, `servermaxbytespersecond`, `name`, `servername`, `serverpassword`, `gametype`, `invertmouse`, `maxnumofplayers`, `roundtime`, `setcpuaffinity`, `voicechat`, `voiceholdtotalk`, `voicekey`, `voicemb`, `voicemin`, `voiceboost`, `help`, `occlusion`.

After the keys, `voiceboost_scale` is `voiceboost / 100`, kept between 0.125 and 4.0.

`Option` fields `unused_d0` and `unused_d4` get the value 2, and `Config.unused_1c` gets 1. Nothing reads them. `setcpuaffinity` is read from the file, but no code uses it.

## Profiler timers

The profiler has 256 `Timer` records (0x58 bytes). A timer has a name, the start count of `rdtsc`, a total and a total in milliseconds. `TimerList` holds the index of the timer of each section of the tick.

`setuptimers` (0xb540) numbers the sections from 0x40 in a fixed order and stores the next free number (0x65) in `numoftimers`. The order is: the packet build, the humans, the objects, the bond tests and bond steps, the render sections, `physicssim` (0x5f) and the interior build steps. `stoptimer` (0xb3f0) adds the cycles since the start of a timer to its total.

## File lists

`FileList` holds 8 lists. A list has a count and 1024 names of 64 bytes (0x10004 bytes).

| Function | RVA | Use |
| --- | --- | --- |
| `listfiles` | 0xd3a0 | Fills a list from a folder search, with files or with folders, and sorts it. Folders that start with a dot do not go in. |
| `removeflieextentions` | 0xd680 | Cuts each name at its first dot. |
| `comparestrings` | 0xd320 | The sort order: a compare of unsigned bytes. |

Odd behavior:

- File lists hold at most 1023 names. Each name has at most 63 bytes before its final zero.
- Short names keep the bytes after their final zero.
- A name with no dot and no 0 in 64 bytes loses the first byte of the next name.

## Radix sort

`csradixsort` (0xac00) sorts `Radix.count` keys in ascending order, as unsigned numbers. It makes four passes of one byte each, from the low byte up. `Radix.order` holds the item numbers in key order and ends with -1. The work arrays `temp`, `bucket_count` and `bucket_offset` keep the values of the last pass. The server sorts the scores of the humans and the objects with it for each player in each tick.

## Number text

`sprintf_comma` (0x8880) writes a number with commas between the groups of three digits. A negative number has no commas.

## Clock and process

| Function | RVA | Use |
| --- | --- | --- |
| `getcpufrequency` | 0xb4a0 | Reads the frequency of the performance counter and stores the length of one count in milliseconds in `SysInfo.ms_per_tick`. |
| `systemtimer` | 0x5350 | The performance counter in milliseconds, cut to 32 bits. The product of the count and the length of one count is cut, not rounded. The main loop uses it. |
| `gettimeofday` | 0xb230 | Seconds and microseconds of the clock, and the time zone. Not used by the game. |
| `testtimer` | 0x18570 | The time of `gettimeofday` in milliseconds. Not used by the game. |
| `setcpuaffinity` | 0xb300 | Binds the process to the first CPU. Not used by the game. |
| `__aulldiv` | 0x5cf70 | The 64-bit unsigned divide of the compiler. The clean version takes the four words as four arguments. Only `gettimeofday` calls it. |

`gettimeofday` subtracts the offset between the Windows time and the Unix time in microseconds from a count of 100 ns steps. The seconds that it returns are about 330 years too big, and the 32-bit seconds field cuts them. For most clock values `testtimer` then gives -2147483648, because the milliseconds do not fit in an int.

## Log files

`debug_resetlogfile` (0x5280) writes `dedlog.txt` with one line when the debug flag is on. The flag is 0 in the game, so it does nothing.

`server_writelogfile` (0x1420) writes `stats.txt`: the number of missions and bullets, the number of accounts, the total and the average play time, and then one line `index:name:time` for each account. The play time is in units of 0.1 s. With no account, the clean code writes 0 as the average. The game writes a leftover register value.

## Main loop

`sr_getpaths` is the main function. It does these steps:

1. Resets the debug log, reads the frequency of the counter and numbers the timers.
2. Sets the pointers that the game uses to read and write files.
3. Stores the start folder and the settings folder.
4. Loads the settings.
5. Starts WinSock and calls `sr_dedicatedserverloop`.

`sr_dedicatedserverloop` sets the server up and then runs ticks without end.

Setup:

1. It asks the master server for the list. It keeps the round time between 1 and 30 hours and the number of players between 2 and 128.
2. It copies the server name and sets the wait time (3600 ticks) and the end time (1260 ticks).
3. It opens the socket, seeds the random numbers with the clock, loads the accounts and the level, and starts the game.
4. It sets the price of four stocks to 100 and clears all players.

Tick (every 16 ms):

1. When the game state is 2, each bot player runs `playerai`.
2. `servercycle` and `serverrecv` handle the network.
3. The `physicssim` timer starts. `playersimulation` runs.
4. In the states 0, 1 and 2 the bodies run in this order: particles, objects, humans, items, bonds, item logic and the four time-to-live functions.
5. `logicsimulation` runs and the `physicssim` timer stops.
6. Every 64 ticks the physics time goes to the log text.
7. `serversend` sends the packets. The tick counter grows.
8. In the states 0, 1 and 2 the bullets run. Then `eventsimulation` runs. The bullet time-to-live runs when the state before `eventsimulation` was 0, 1 or 2.

Schedule:

- The loop sleeps 1 ms, then reads the clock.
- When the clock is 16 ms or more past the next tick time, it runs a tick. It runs at most 5 ticks in a row.
- The next tick time grows by 16 ms for the first three ticks. From the fourth tick on, it restarts from the clock.
- The sleep follows the system timer resolution, so the ticks have some jitter.
- Every minute the loop writes a status line with the tick rate, the players and the bots.
