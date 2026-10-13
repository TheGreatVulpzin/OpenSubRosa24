# Verification of side-effect functions

Status: design. No code is written yet.

The lifter marks 52 functions `side_effect`. The verifier cannot test them, because it runs each function twice (the lifted version, then the original). A second run would send a second packet, write a file twice, or read a different clock value. This document lists the 52 functions, groups them by import, and proposes a record and replay test.

## Result in short

- 52 of 340 functions are `side_effect`. 268 are `lifted`, 16 are `blocked` and 4 are `handwritten`.
- 10 of the 52 are MSVC CRT startup and exception code (RVA 0x5ce18 and up). Nobody lifts them. They leave the scope.
- 42 are game functions. They fall into 5 groups: network (10), file (23), time (4), heap (4) and one other (1).
- One design covers all groups. The hook records every import call. The harness replaces the imports with stubs that return the recorded results. Both versions run against the same record.
- The original run is the oracle. A call where the original does not match its own record is skipped. A call where the lifted version does not match is a real difference.

## How the list was made

1. Export from Ghidra: `LiftExport.java` (see `docs/lift.md`, "Pipeline", step 1). The result has 340 functions.
2. Run `python -I tools/lift/lift.py` without `--compile`.
3. Read `build/lift/report.csv`. The rows with status `side_effect` are the 52 functions.
4. A helper script (not committed) walks the call graph again. It writes `analysis/024d/side_effects.csv`. The CSV has the own imports, all imports that the function reaches, and the call path to one import.

`lift.py` calls a function impure when it reaches an import that is not in `PURE_IMPORTS`. The rule is transitive: a caller of an impure function is impure.

### Findings about the list

1. **The `reason` column shows WinSock ordinals.** The 024d imports WinSock by ordinal. `Ordinal_8` is `htonl`, `Ordinal_9` is `htons` and `Ordinal_15` is `ntohs`. These three are pure. The report names them for `sendpacket`, `servercycle`, `serverrecv` and `serversend`. These functions are still impure, because they also call `sendto` (ordinal 20) or `recvfrom` (ordinal 17). So no function leaves the list, but the reason is wrong. Map the ordinals in `lift.py` (see "Code changes").
2. **Two game functions are imports in disguise.** `getpaths` (0x5cd10) copies the `fread` and `fwrite` addresses from the import table (IAT slots at RVA 0x5e0d8 and 0x5e0d4) into two globals. The globals are at RVA 0x91022a4 and 0x13bf2d00. `loadblock` and other readers call `fread` and `fwrite` through these pointers. A patch of the IAT alone does not cover a snapshot, because the snapshot holds the old pointers. See "Replay" below.
3. **The game never reads `errno`.** No function calls `_errno`, `ferror`, `feof` or `WSAGetLastError`. The test does not need to record `errno`.
4. **`getpaths` ends with a jump to `dedicatedserverloop`.** The server loop is the whole game. It is not a unit. See the risks.
5. **Some functions belong to the client.** The 024d binary has client code. `saveaccountsclient`, `loadaccountsclient`, `clientrecv`, `leveleditor_save` and `leveleditor_load` are probably never called on a server. The log counts calls for each hook (`sv_note_call`). A recording shows which ones run.

## The 52 functions

"Own impure imports" are the imports that the function calls itself. "First path through callees" shows how the function reaches an import it does not call itself. `analysis/024d/side_effects.csv` has the complete data.

| Group | RVA | Function | Own impure imports | First path through callees | Lifter reason |
|---|---|---|---|---|---|
| Network | 0xd710 | disablesocket | `bind`, `ioctlsocket`, `socket` | - | `Ordinal_2` |
| Network | 0xd860 | recvpacket | `recvfrom` | - | `Ordinal_17` |
| Network | 0xd910 | sendpacket | `sendto` | - | `Ordinal_8` |
| Network | 0x1ac40 | getmasterserverip | 13 imports: `__WSAFDIsSet`, `_sleep`, `closesocket`, ... | - | `_sleep` |
| Network | 0x2e310 | clientrecv | - | clientrecv → recvpacket (`recvfrom`) | `Ordinal_17` |
| Network | 0x33570 | servercycle | - | servercycle → sendpacket (`sendto`) | `Ordinal_8` |
| Network | 0x33650 | serverrecv | `sendto` | serverrecv → recvpacket (`recvfrom`) | `Ordinal_15` |
| Network | 0x4ed30 | serversend | `sendto` | - | `Ordinal_8` |
| Network | 0x5c8d0 | dedicatedserverloop | 8 imports: `QueryPerformanceCounter`, `_sleep`, `_time64`, ... | dedicatedserverloop → getmasterserverip (`__WSAFDIsSet`), +21 more | `fopen` |
| Network | 0x5cd10 | getpaths | `WSAStartup`, `_getcwd` | getpaths → dedicatedserverloop (`QueryPerformanceCounter`), +30 more | `Ordinal_115` |
| File | 0x1080 | saveaccountsserver | `fclose`, `fopen` | - | `fclose` |
| File | 0x11d0 | saveaccountsclient | `_chdir`, `fclose`, `fopen` | - | `_chdir` |
| File | 0x12f0 | loadaccountsclient | `_chdir`, `fclose`, `fopen` | - | `_chdir` |
| File | 0x1420 | server_writelogfile | `fclose`, `fopen`, `fprintf` | - | `fprintf` |
| File | 0x29a0 | loadblock | `fclose`, `fopen` | - | `fclose` |
| File | 0x3740 | building_save | `fclose`, `fopen` | - | `fclose` |
| File | 0x5280 | debug_resetlogfile | `fclose`, `fopen`, `fprintf` | - | `fopen` |
| File | 0x8320 | leveleditor_save | `fclose`, `fopen` | - | `fopen` |
| File | 0x8590 | leveleditor_load | `fclose`, `fopen` | - | `fclose` |
| File | 0xd020 | loadtextfile | `fclose`, `fgetc`, `fopen` | - | `fclose` |
| File | 0xd3a0 | listfiles | `_findclose`, `_findfirst64i32`, `_findnext64i32` | - | `_findclose` |
| File | 0xe2e0 | loadaccountsserver | `fclose`, `fopen` | - | `fclose` |
| File | 0x13d80 | building_load | `fclose`, `fopen` | - | `fclose` |
| File | 0x185a0 | newspaper_load | - | newspaper_load → loadtextfile (`fclose`), +2 more | `fclose` |
| File | 0x2a510 | area_createinteriorblocks | `_chdir` | area_createinteriorblocks → listfiles (`_findclose`), +6 more | `_chdir` |
| File | 0x3e070 | area_createsblocks | `_chdir` | area_createsblocks → listfiles (`_findclose`), +6 more | `_chdir` |
| File | 0x4e7a0 | loadsaveoptions | `_chdir`, `_time64` | loadsaveoptions → loadtextfile (`fclose`), +2 more | `_time64` |
| File | 0x502a0 | setupgame | `_chdir` | setupgame → listfiles (`_findclose`), +6 more | `_chdir` |
| File | 0x54e40 | createlevel | - | createlevel → building_load (`fclose`), +3 more | `fclose` |
| File | 0x590f0 | resetgame | `_time64` | resetgame → saveaccountsserver (`fclose`), +4 more | `_time64` |
| File | 0x5ae70 | logicsimulation_round | - | logicsimulation_round → resetgame (`_time64`), +5 more | `_time64` |
| File | 0x5b9b0 | logicsimulation_world | - | logicsimulation_world → resetgame (`_time64`), +6 more | `fclose` |
| File | 0x5c580 | logicsimulation | - | logicsimulation → resetgame (`_time64`), +6 more | `_time64` |
| Time | 0x5350 | systemtimer | `QueryPerformanceCounter` | - | `QueryPerformanceCounter` |
| Time | 0xb230 | gettimeofday | `GetSystemTimeAsFileTime`, `__daylight`, `__timezone`, `_tzset` | - | `GetSystemTimeAsFileTime` |
| Time | 0xb4a0 | getcpufrequency | `QueryPerformanceFrequency` | - | `QueryPerformanceFrequency` |
| Time | 0x18570 | testtimer | - | testtimer → gettimeofday (`GetSystemTimeAsFileTime`), +3 more | `GetSystemTimeAsFileTime` |
| Heap | 0x1e50 | landscape_buildsectorlods | `free`, `malloc` | - | `free` |
| Heap | 0xf2b0 | createarea | `free`, `malloc` | - | `malloc` |
| Heap | 0x29bf0 | area_block_createbuildinginterior | - | area_block_createbuildinginterior → landscape_buildsectorlods (`free`), +1 more | `free` |
| Heap | 0x46c30 | building_buildfloorplan | - | building_buildfloorplan → area_block_createbuildinginterior → landscape_buildsectorlods (`free`), +1 more | `free` |
| Other | 0xb300 | setcpuaffinity | `GetCurrentProcess`, `GetProcessAffinityMask`, `SetProcessAffinityMask` | - | `SetProcessAffinityMask` |
| CRT | 0x5ce18 | __security_check_cookie | - | __security_check_cookie → __report_gsfailure (`GetCurrentProcess`), +5 more | `UnhandledExceptionFilter` |
| CRT | 0x5d08c | __tmainCRTStartup | 9 imports: `HeapSetInformation`, `InterlockedCompareExchange`, `InterlockedExchange`, ... | __tmainCRTStartup → getpaths (`WSAStartup`), +32 more | `Sleep` |
| CRT | 0x5d316 | mainCRTStartup | - | mainCRTStartup → __security_init_cookie (`GetCurrentProcessId`), +45 more | `QueryPerformanceCounter` |
| CRT | 0x5d320 | __report_gsfailure | 6 imports: `GetCurrentProcess`, `IsDebuggerPresent`, `SetUnhandledExceptionFilter`, ... | - | `UnhandledExceptionFilter` |
| CRT | 0x5d436 | __CxxUnhandledExceptionFilter | `terminate` | - | `terminate` |
| CRT | 0x5d492 | __onexit | 5 imports: `DecodePointer`, `EncodePointer`, `__dllonexit`, ... | __onexit → _unlockexit (`_unlock`) | `EncodePointer` |
| CRT | 0x5d52a | _unlockexit | `_unlock` | - | `_unlock` |
| CRT | 0x5d533 | _atexit | - | _atexit → __onexit (`DecodePointer`), +5 more | `EncodePointer` |
| CRT | 0x5d769 | __except_handler4 | `except_handler4_common` | - | `except_handler4_common` |
| CRT | 0x5d7f0 | __security_init_cookie | 5 imports: `GetCurrentProcessId`, `GetCurrentThreadId`, `GetSystemTimeAsFileTime`, ... | - | `QueryPerformanceCounter` |

## Groups

| Group | Count | Imports | Idea |
| --- | --- | --- | --- |
| Network | 10 | `sendto`, `recvfrom`, `socket`, `bind`, `ioctlsocket`, `closesocket`, `connect`, `send`, `recv`, `select`, `gethostbyname`, `getservbyname`, `getsockopt`, `WSAStartup`, `__WSAFDIsSet` | Compare the bytes that leave. Replay the bytes that arrive. |
| File | 23 (25 reach it) | `fopen`, `fclose`, `fprintf`, `fgetc`, `fseek`, `printf`, `fread` and `fwrite` (through globals), `_chdir`, `_getcwd`, `_findfirst64i32`, `_findnext64i32`, `_findclose` | Compare the bytes that go to a file. Replay the bytes that come from a file. |
| Time | 4 (12 reach it) | `QueryPerformanceCounter`, `QueryPerformanceFrequency`, `GetSystemTimeAsFileTime`, `_time64`, `_sleep`, `_tzset`, `__timezone`, `__daylight` | Replay the clock. The call is safe to run twice, but the value changes. |
| Heap | 4 (14 reach it) | `malloc`, `free` | Give both versions the same blocks. Compare the blocks after the run. |
| Other | 1 | `GetCurrentProcess`, `GetProcessAffinityMask`, `SetProcessAffinityMask` | Record the arguments. Replay the return. |

The first count is the group of the first match (network, then file, then time, then heap). The count in parentheses includes all callers. Many composite functions reach several groups. `resetgame` reaches time, file and heap. The stubs for all groups are active in each test, so the group only decides the order of the work.

### What to record for each import

Rule for all imports: record the arguments before the call and the outputs after the call. A "handle" is a value that the game stores and passes back later (a socket, a `FILE *`, a find handle). The harness compares a handle as a plain number. A pointer to the stack or the heap is never compared as a number. The test compares the bytes behind the pointer.

| Import | Record before the call | Record after the call | Replay returns | Compare |
| --- | --- | --- | --- | --- |
| `sendto` | socket, flags, length, `to` (`tolen` bytes), data (`length` bytes) | return | The recorded return | Flags, length, address bytes and data bytes. They must match exactly. |
| `send` | socket, flags, length, data | return | The recorded return | The same as `sendto`. |
| `recvfrom` | socket, buffer size, flags, `*fromlen` | return, data (return bytes), `from`, `*fromlen` | Copies the recorded data and address. Returns the recorded return. | Socket, buffer size, flags, `*fromlen` |
| `recv` | socket, buffer size, flags | return, data | The same as `recvfrom` | Socket, buffer size, flags |
| `socket`, `closesocket`, `connect`, `bind` | all arguments, address bytes | return | The recorded return (a handle for `socket`) | All arguments and the address bytes |
| `ioctlsocket`, `getsockopt` | arguments, input value | return, output value | The recorded return and output value | The arguments and the input value |
| `select` | `nfds`, the three `fd_set` blocks, the `timeval` | return, the three `fd_set` blocks after the call | Writes the recorded sets | `nfds`, input sets, `timeval` |
| `__WSAFDIsSet` | socket, the set | return | The recorded return | Socket and set |
| `gethostbyname`, `getservbyname` | the name string | return, the whole result (`h_length`, each address in `h_addr_list`, or the port) | A `hostent` or `servent` that the stub builds | The name string |
| `WSAStartup` | version | return | The recorded return | The version |
| `fopen` | path string, mode string | return (as a handle) | A fake `FILE *` from a fixed table of the stub | Path and mode strings |
| `fclose` | handle | return | The recorded return | Handle |
| `fgetc` | handle | return | The recorded return | Handle |
| `fread` (through the global) | buffer, size, count, handle | return, data (return times size bytes) | Copies the recorded data | Size, count, handle |
| `fwrite` (through the global) | buffer, size, count, handle, data | return | The recorded return | Size, count, handle, data bytes |
| `fprintf`, `printf` | handle, format string, the text after formatting | return | The recorded return | The formatted text. The stub formats the arguments itself. |
| `fseek` | handle, offset, origin | return | The recorded return | All arguments |
| `_chdir` | path string | return | The recorded return. The stub does not change the directory. | Path string |
| `_getcwd` | size | return, path string | Copies the recorded string | Size |
| `_findfirst64i32` | mask string | return, `_finddata64i32_t` (record 512 bytes) | Copies the record | Mask string |
| `_findnext64i32` | handle | return, `_finddata64i32_t` | Copies the record | Handle |
| `_findclose` | handle | return | The recorded return | Handle |
| `QueryPerformanceCounter`, `QueryPerformanceFrequency` | none | return, 8 bytes | Copies the record | Nothing |
| `GetSystemTimeAsFileTime` | none | 8 bytes | Copies the record | Nothing |
| `_time64` | pointer is null or not | return (64 bits), 8 bytes if not null | Copies the record | Whether the pointer is null |
| `_sleep` | milliseconds | none | Returns at once | Milliseconds |
| `_tzset` | none | none | Does nothing | The call happens |
| `__timezone`, `__daylight` | none | the integer behind the returned pointer | A pointer to an integer that the stub owns | The call happens |
| `malloc` | size | the return is null or not | A block of the stub pool (see below) | Size |
| `free` | pointer is null or not | none | Does nothing | The call happens |
| `GetCurrentProcess`, `GetProcessAffinityMask`, `SetProcessAffinityMask` | all arguments | return, the two masks | The recorded return and masks | Arguments |
| `inet_addr` | the string | return | Runs the real function (it is pure) | The call happens |

Notes:

- **Record `errno`: no.** See finding 3.
- **Pure imports.** Add `inet_addr` and the ordinals 8, 9, 14 and 15 to the pure list. The stub for `inet_addr` calls the real function.
- **Time call order.** The record keeps the order of the calls. The first `QueryPerformanceCounter` call in a test gets the first recorded value.
- **Heap blocks.** The recorded address is not useful. The block of the original session is not in the snapshot. The stub gives out blocks from its own pool and resets the pool before each run. The lifted run and the original run then receive the same addresses, and the `.data` pointers match. After the lifted run, the harness copies each block. After the original run, it compares the blocks. This also covers writes to the heap, which the live verifier cannot see today.
- **Strings and sizes.** Record a string up to its NUL byte (maximum 260 bytes). Record a data buffer in full up to 64 KB. A larger buffer (for example a `fread` of a map file) goes to a side file (`sv_io.bin`), and the event line holds only the offset and the length.

## Design

### Record

1. In a play session with `original_only=1`, the DLL replaces the import slots of the 024d with wrappers. Each wrapper calls the real function. If a record scope is open, it also writes one event to the memory log of the scope.
2. A scope opens when a hooked side-effect function starts (the thunk already calls `sv_record_args`). The scope closes when the hook returns. A scope can nest. For example, `logicsimulation` calls `resetgame`. Each open scope gets its own copy of the events.
3. When a scope closes, the DLL decides if it keeps the call. It hashes the arguments and the events. It keeps up to 24 different calls for each function. Today the dedupe uses the arguments alone. That is wrong for `systemtimer()`, which has no arguments, and for a function whose result depends on file content.
4. The DLL writes the call to `sv_args.txt`. The `R` line stays as it is today. Event lines follow it:

   ```
   R 1080 V:0
   E fopen V:0 s:646174612f6163636f756e74732e747874 s:7762 r:V:2f3c1a0
   E fwrite V:1 V:2e V:1 h:V:2f3c1a0 b:6e616d653d...  r:V:1
   E fclose h:V:2f3c1a0 r:V:0
   Z r:V:0
   ```

   `s:` is a string, `b:` is a byte block that the function reads, `o:` is a byte block that the import fills, `h:` is a handle, `V:` is a dword, `r:` is the return. `Z` closes the call and holds the return of the function.
5. Packets keep going to `sv_packets.bin` (`capture.c`). That file stays a separate tool for people. The events hold their own copy of the bytes, because replay needs them in order with the other imports.

### Replay

1. `resolve_imports` in the harness puts a stub in the IAT for each import in the table above (a replay switch turns this on). The stub for an import that is not in the table is the real function.
2. After `apply_snapshot`, the harness writes the stub addresses of `fread` and `fwrite` into the two globals (RVA 0x91022a4 and 0x13bf2d00). The harness repeats this each time it calls `apply_snapshot`.
3. For each recorded call, the harness sets the replay cursor to the first event. It runs the lifted version. It checks the cursor, resets it, undoes the writes and runs the original. It checks the cursor again.
4. Each stub does three things in this order:
   1. It reads the next event. If the import or the event type does not match, it raises a divergence (`0xE0535254`) and the run stops.
   2. It compares the input arguments with the record. A mismatch is a divergence. The stub keeps the first one (event number, import, field, expected bytes, actual bytes).
   3. It writes the recorded outputs and returns the recorded result.
5. At the end of a run, an event that nobody used is also a divergence ("missed call").

### Verdict

| Original vs record | Lifted vs record | Result |
| --- | --- | --- |
| Match | Match | Same. The harness still compares `.data`, return value and pointer memory as today. |
| Match | Divergence | **Difference.** Print the first divergence. |
| Divergence | Any | Skipped ("not replayable"). The snapshot or the record does not fit this call. |
| Match | Match, but `.data`, return or heap blocks differ | **Difference.** |

The record is a middle step: both versions get compared to the same facts. This also detects a lifted function that sends the right bytes in a different order, which a comparison of final memory would miss.

## Code changes

Smallest change for each part. No code is written yet.

### `tools/lift/lift.py` and `lifted_table`

1. In `main()`, do not stop at `rva in side`. Lift the function. If it compiles, give it the status `replay`. Keep the CRT and the two non-units (`dedicatedserverloop`, `getpaths`) as `side_effect`.
2. `gen_hooks()` makes a record-only wrapper for `replay` functions. The wrapper calls the original and does nothing else. The thunk stays as it is: it calls `sv_record_args`, which opens the scope. The wrapper adds one call, `sv_record_end(ret)`, before it returns. The live server never runs the lifted version of these functions.
3. `gen_table()` adds one field to `LfEntry`: `int replay`. The harness runs entries with `replay=1` only with the stubs on.
4. Map the WinSock ordinals in the import list (a table of 30 lines). Put `htonl`, `htons`, `ntohl`, `ntohs` and `inet_addr` in the pure set. Then the `reason` column names the real import.

### `hook/record.c` and `hook/record.h`

1. Add `sv_record_begin`, `sv_record_event` and `sv_record_end`. The scope stack is an array of 8 entries. Each entry has a memory buffer (limit 1 MB, a call over the limit is dropped).
2. Move the write of the `R` line from `sv_record_args` to `sv_record_end`, and add the dedupe by hash of arguments and events.
3. Add the event writer. It has one function for each kind of field (`V`, `s`, `b`, `o`, `h`, `r`).
4. The import wrappers (about 30, in a new file `hook/record_imports.c`) call the real function and the event writer. The wrappers for `fprintf` and `printf` format the text with the `vsnprintf` of `MSVCR100.dll`.

### `hook/capture.c` and `hook/proxy.c`

1. `capture.c` can stay as it is. It records packets for tools, not for replay.
2. `proxy.c` already forwards `sendto` and `recvfrom` through C (`sv_sendto_capture`, `sv_recvfrom_capture`). Add a call to `sv_record_event` in these two functions. Today the proxy forwards the other WinSock functions with a plain `jmp`. Write a C wrapper for the 12 that the table needs (`socket`, `bind`, `ioctlsocket`, `closesocket`, `connect`, `send`, `recv`, `select`, `gethostbyname`, `getservbyname`, `getsockopt`, `__WSAFDIsSet`) and keep `WSAStartup` and the pure ones as `jmp`.
3. The proxy sits between the game and WinSock. The MSVCR100 and KERNEL32 imports have no such place. Patch the IAT of the 024d (`VirtualProtect`, then write the slots) when the DLL starts. The loader normally fills the IAT before it runs `DllMain`, and `getpaths` runs later, so the copies of `fread` and `fwrite` in the two globals are the wrappers too. Test this on the first run: log the two globals after `getpaths`. A small function `sv_patch_iat(dll, name, wrapper)` in `hook/imports.c` does this. It reuses the walk of the import table that `sr_import` already has.

### `src/tests/harness.c`

1. `load_calls`: read the `E` and `Z` lines into a list in `Call`. The parser needs one decoder for the field kinds.
2. `resolve_imports`: after `GetProcAddress`, ask `stub_for(dll, name)`. The function returns a stub, or `NULL` for the real function. The WinSock imports are by ordinal, so `stub_for` needs the ordinal table.
3. A new file `src/tests/stubs.c` holds the stubs, the cursor, the divergence report and the block pool. Each stub is a few lines: `next_event`, `check_in`, `put_out`.
4. In `main()`, call `apply_snapshot` and then `patch_io_pointers()`. Around `safe_invoke`, call `rep_begin(call)` and `rep_end()`. Extend the verdict logic as in the table above.
5. Add `HARNESS_REPLAY_DEBUG=1`. It prints each event with the actual arguments.

### What stays the same

`verify.c` and the live verifier do not change. `hook_wrap.h` does not change: replay functions use their own small wrapper.

## Order of work

1. **Time (4 functions), one session.** The stubs are small. `systemtimer` and `getcpufrequency` need no argument checks. This tests the whole chain: scope, event line, parser, stub, verdict.
2. **Heap (4).** Adds the block pool and the block comparison.
3. **Network leaves (`recvpacket`, `sendpacket`, `serversend`).** Needs the wrappers in `proxy.c`. Then `servercycle` and `serverrecv`, which use the same stubs.
4. **File leaves** (`loadtextfile`, `loadblock`, `saveaccountsserver`, `server_writelogfile`, and others). Adds `fprintf`, the two global pointers and the side file for large reads.
5. **Composite functions** (`resetgame`, `setupgame`, `logicsimulation*`, `createlevel`, `area_create*`). They work with no new stub, but their records are long. The risks below apply most to them.

## Open risks

1. **The snapshot is older than the call.** The harness loads one `.data` snapshot (at `snapshot_after`). The calls come from the whole session. For a pure function this is fine in practice. For a function with state (`resetgame`, `logicsimulation`), the original may take another path than in the recording. Then the original diverges and the call is skipped. If many calls are skipped, take a new snapshot at the start of each recorded call for these functions. Store only the pages that changed since the first snapshot.
2. **Large inputs.** `loadblock` and `createlevel` read big files. The record can grow to hundreds of MB. Use the side file `sv_io.bin` and keep few different calls (maybe 3) for each of these functions.
3. **Private data.** The records hold account files, player names, chat and possibly `config.txt` (it has passwords). They stay in `build/`, like the other recordings. Never commit them. Do not copy their content into docs or issues.
4. **Time loops.** A function that waits for the clock to advance reads the clock many times. A short record ends before the loop ends. The stub reports "events used up" and the original diverges, so the call is skipped. The cost is lost coverage. A fix is to extend the last time value by a fixed step when the record ends. Do not do this first.
5. **`dedicatedserverloop` and `getpaths`** run the whole server. Their record would be a full session. Keep them as `side_effect` and test their parts.
6. **Hidden impurity.** The import list does not show instructions that read machine state, such as `RDTSC` (the profiler timers, which the harness already ignores) and `CPUID`. A function that reads one of them is outside this design. A scan of the machine code for `RDTSC` in the 268 `lifted` functions would find them. The harness has not shown a difference for them so far.
7. **Wrapper changes behavior.** An IAT wrapper adds code between the game and the import. The code must keep the registers and the x87 state that the caller expects (see `docs/lift.md`, "What the 024d does that breaks the normal ABI"). Write the wrappers as plain `__cdecl` and `__stdcall` C functions. Run a play session with and without the wrappers, and compare the packets (`sv_packets.bin`).
8. **Output size of structs.** The sizes of `fd_set`, `hostent`, `servent` and `_finddata64i32_t` come from the CRT header of the compiler. Check them against MSVC 2010, the compiler of the 024d (`MSVCR100.dll`).
9. **Code that does not run on the server.** If the client functions never run, the record has no call for them. The report shows "0 calls" for them. They stay unverified and the original keeps running. This is acceptable: the server does not use them.
10. **Variable arguments.** `fprintf` and `printf` read their arguments from the stack. The wrapper and the stub must read the same number of bytes that the format string asks for. A wrong format string in the game would crash the stub in the same way it crashes the original.
11. **Recorded handles.** A `FILE *` in the record is a fake token in the replay. If a function stores the pointer in `.data` and compares it with the snapshot value, the compare can fail. The harness already compares both versions to each other, so the token is the same in both. The risk is only a false difference against the snapshot. Watch for it in the first runs.
