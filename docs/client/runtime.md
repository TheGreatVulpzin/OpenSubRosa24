# Debug and timers

Files: `src/client/debug/debug.c`, `src/client/setup/timers.c`, `src/client/setup/game.c`.

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

## Small hooks

- `updateplayticket` has no code.
- `reactivategame` turns on the relative mouse mode of SDL when `cl_data_relative_mouse` is not 0. Then it makes the frame textures and the shader programs again.

## JPEG restart

`reset` (in `src/client/image/jpeg.c`) empties the bit buffer of the JPEG decoder, clears the DC predictors of the first three components and the pending marker, and starts the restart counter again. With no restart interval the counter is 0x7fffffff.

## Processor cycles and argument text

`readprocessorcycles` writes the low and high words of the processor cycle counter to its output buffer. Each call reads a new count.

`os_time64` supplies the same CRT time service to the client and server. The unsigned division and remainder helpers in `src/common/system.c` accept four words and return two words.

`unescapequotes` removes a backslash directly before a quote in the command line text. The next scan starts one byte after the current position. Adjacent quotes can therefore remain unprocessed after a removal.

## Application ticket callbacks

`AppTicket` holds a callback table, callback flags and type, two words of a pending call, a receiver, a member method and a ticket callback. Its constructor initializes the call result base and clears the ticket callback.

`callresult_run` clears both call words and invokes the member method with no I/O failure. `callresult_runhandle` does this only when both supplied call words match. The receiver enters the method in ECX. The callback response occupies four bytes.

`appticket_destroy` invokes the call result destructor. `appticket_cleanup` applies that destructor to the global application ticket during process exit.
