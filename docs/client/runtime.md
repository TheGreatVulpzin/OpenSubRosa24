# Debug, timers and Steam

Files: `src/client/debug/debug.c`, `src/client/setup/timers.c`, `src/client/setup/game.c`, `src/client/steam/steam.c`.

## Debug display

The debug record (`sr_data_debug`) holds the data that the debug display shows.

- **Crosses.** `debug_addcross(center, size)` adds three lines: one along each axis, `size` long on both sides of the center. Each cross adds 3 to `Debug.num_lines`. The function does nothing when `num_lines` is 0xffd or more. The array has room for 4096 lines.
- **Named values.** `debug_addataint(name, value)` and `debug_addatafloat(name, value)` keep a list of named values in `Debug.values`. A name that is in the list gets the new value, and the last entry with that name wins. A new name goes at the end of the list. The integer function stops adding names at 128 entries. The float function stops at 32 entries. Both functions share the list, so a float value can sit in an entry that holds an integer. `is_float` tells which value the display reads.
- **Flag.** The two value functions do nothing when `sr_data_debugit` is 0. `debug_addcross` does not read the flag.
- **Log file.** `debug_writelogfile(text)` opens the file named in `Debug.log_path` for update, goes to its end, writes the text and a carriage return and a line feed, and closes the file. The file must exist. `Debug.log_file` holds the handle, and it holds 0 when the file cannot be opened.

## Timers

A timer (`sr_data_timer[256]`, 0x58 bytes each) has a name of up to 63 characters, the processor cycle count at its start, a total of cycles and a total in milliseconds.

- `resettimer(index, name)` copies the name and clears the total of cycles. The milliseconds keep their value. A name of 64 characters or more runs into the start of the cycle count.
- `starttimer(index)` stores the processor cycle count as the start.

## Steam

`steam_on` (`cl_data_steam_on`) is 1 after `steam_init` succeeds.

- `steam_init` asks Steam to restart the game through the Steam client. If Steam does not restart it, `steam_init` starts the Steam interface. The result is 1 when Steam restarts the game or starts, and 0 when the interface does not start.
- `steam_shutdown` stops the interface and clears the flag. `steam_update` runs the callbacks when the flag is set.
- `steam_getname(buffer)` copies the Steam name of the player and returns 1. With the flag clear it returns 0 and does not write.
- `steam_getid(id)` writes the 64 bit Steam id of the player. With the flag clear it does not write.

`steam_requestticket` and `steam_requestauthticket` request a ticket from Steam.

## Small hooks

- `updateplayticket` has no code.
- `reactivategame` turns on the relative mouse mode of SDL when `cl_data_relative_mouse` is not 0. Then it makes the frame textures and the shader programs again.

## JPEG restart

`reset` (in `src/client/image/jpeg.c`) empties the bit buffer of the JPEG decoder, clears the DC predictors of the first three components and the pending marker, and starts the restart counter again. With no restart interval the counter is 0x7fffffff.
