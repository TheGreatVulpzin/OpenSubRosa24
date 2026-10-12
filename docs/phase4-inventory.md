# Phase 4 inventory: what the 024d uses besides its own game code

Goal: list everything that `subrosadedicated024d.exe` takes from outside its own game functions, so that Phase 4 (`subvulpes-server`, no original `.exe`) knows what to replace. See `docs/plano.md`, Phase 4.

Target: `original/blackhouser/subrosadedicated024d.exe` (PE32, SHA-256 `11e00678…`). All addresses are **RVAs** (module base `0x400000`). Function names come from `analysis/024d/functions.csv` unless a note says otherwise.

## 0. How this inventory was made

| Step | Tool | Result |
| --- | --- | --- |
| 1 | `bash tools/cloud/setup.sh`, `bash tools/ghidra/analyze_all.sh` | Ghidra 12.1.4 project and `build/features/*.jsonl`. Both scripts ended with exit code 0. |
| 2 | A small PE parser (not committed) | The import table: 102 imports, with the IAT slot of each one. |
| 3 | `objdump -d -M intel -j .text` of the `.exe` | 106,142 decoded instructions. Every `call [IAT]`, and every `call`/`jmp` to one of the 101 `jmp [IAT]` thunks, was attributed to a function. |
| 4 | Static reachability from the entry point `0x5d316` | Direct calls, address immediates, and the CRT initialiser tables. It tells live code from code that nothing references. |
| 5 | Ghidra headless decompile (`tools/ghidra/DecompileFunctions.java`) | Used to confirm `getmasterserverip`, `server_writelogfile`, `loadsaveoptions`, `saveaccountsserver`, `loadaccountsserver`, `leveleditor_load`, `newspaper_load`, `servercycle`. |
| 6 | Symbol sizes of the Linux 0.24c server and the i386 Mach-O (`analysis/structs`, `original/vanilla`) | Capacity and size of the global arrays, and which structs hold pointers. |

Image layout (from the PE header):

| Section | RVA | Virtual size | File size |
| --- | --- | ---: | ---: |
| `.text` | `0x1000` | `0x5c940` | `0x5ca00` |
| `.rdata` | `0x5e000` | `0x1bd8` | `0x1c00` |
| `.data` | `0x60000` | `0x25e997e0` | `0xe00` |
| `.reloc` | `0x25efa000` | `0x40518` | `0x40600` |

`SizeOfImage` is `0x25f3b000`. Only `0xe00` bytes of `.data` are in the file. The rest is zero memory made by the loader. `.reloc` has 21,953 base relocations, 21,925 of them in `.text`. They are all the absolute addresses of globals and strings that the code uses.

Marks used in the tables:

- `†` : nothing in the program refers to this function (no call, no address immediate, no table entry). For the CRT region (RVA ≥ `0x5ce18`) the mark is less reliable, because the exception tables are not followed.
- `~` : the function is not in `functions.csv`. It is shown as `sub_<rva>`.
- `×n` : n call sites in the same function.

Limits of the method: it is static. A call through a computed pointer cannot be seen. The only computed calls found in the live code are the `freadptr`/`fwriteptr` pointers (section 4), the `qsort` comparator and the CRT tables. All of them were followed by hand.

## 1. Summary

- **102 imports**: 21 from `WSOCK32.dll` (all by ordinal), 61 from `MSVCR100.dll`, 20 from `KERNEL32.dll`. 534 call sites. 7 imports are used only by code that nothing calls: `gethostname`, `_tzset`, `__timezone`, `__daylight`, `GetProcessAffinityMask`, `SetProcessAffinityMask`, `fputc`.
- **The server has no threads, no GUI and no registry access.** The Windows API used is only: timers (`QueryPerformanceCounter`/`Frequency`, `Sleep`), the CRT start-up and crash handler, and WinSock 1.1.
- **Dynamic memory is used at 2 places only** (`createarea`, `landscape_buildsectorlods`). Everything else is in the 606 MiB (`0x25e997e0` bytes) of static `.data`. Only about 111 MiB of it has a name in the CSVs (section 4).
- **Files**: reads `config.txt`, `user/server.srk`, `data/city.sbc`, `data/building/*.sbb`, `data/block/*.sbl` and (optional) `readme.txt`. Writes `user/server.srk` and `stats.txt`. The `dedlog.txt` debug log can never start, because its flag is never set. Details in section 3.
- **About 40 % of the decoded instructions are x87** (41,955 in 271 functions). The only SSE2 use is one `cvttsd2si` in `_ftol2` and 14 `movaps` in `bondsimulation2`. The CRT sets the x87 precision to 53 bits at start-up. This is the biggest risk for an exact port (section 7).
- **The city is made with `srand(0)` and the MSVC `rand()`** (`createlevel`). A port must give the same number sequence (section 7).
- **`functions.csv` covers the live code except 7 functions** (6 in the CRT and the `qsort` comparator at `0xd320`). 32 of its 340 functions are not referenced by live code (client-only or editor code).
- The function `getpaths` (`0x5cd10`) is the real `main`, and `disablesocket` (`0xd710`) really opens the server socket. See the naming notes in section 8.

## 2. Imports

The 024d calls most imports with `call [IAT]`. Some of them are reached only through a `jmp [IAT]` thunk (`memcpy`, `memset`, `_CI*`, all WinSock functions). The thunks are at `0x5cd7c`–`0x5ce17` and `0x5d88c`–`0x5d93a`. The tables count both ways.

### 2.1 Memory

The game code allocates from the heap in two functions only. `createarea` (`0xf2b0`) frees and allocates one block per level area (4 areas, record size `0x100d6c`), and `landscape_buildsectorlods` (`0x1e50`) does the same for 4 buffers in each `area_block` record (record size `0x84`). In both cases the pointer is stored **inside a global struct** (see section 7, R3). `memcpy` and `memset` come from `MSVCR100` through thunks. `memset` clears each new record in the `create*` functions with the size of the struct.

| Import | DLL | Sites | Called from (RVA, name) | Use |
| --- | --- | ---: | --- | --- |
| `malloc` | MSVCR100 | 2 | 0x1e50 landscape_buildsectorlods; 0xf2b0 createarea | Heap block for per-area volume data (`createarea`: x·y·z·4 bytes; pointer kept in the area record) and for the four per-sector buffers of the landscape LODs (`landscape_buildsectorlods`: count·12 bytes each). The only dynamic allocation in the game code. |
| `free` | MSVCR100 | 2 | 0x1e50 landscape_buildsectorlods; 0xf2b0 createarea | Releases the same blocks before they are rebuilt. |
| `memcpy` | MSVCR100 | 15 | 0x1e50 landscape_buildsectorlods ×4; 0x6260 createevent_message; 0xa650 sub_a650~†; 0xd990 cs_pwrite; 0xd9f0 cs_pread†; 0x2e310 clientrecv†; 0x2ebc0 sub_2ebc0~†; 0x33650 serverrecv ×4; 0x4ed30 serversend | Packet assembly and parsing (`serverrecv`, `serversend`, `createevent_message`), voice frames, LOD buffers, `cs_pwrite`. |
| `memset` | MSVCR100 | 18 | 0x4450 building_rotatefloorplan; 0xac00 csradixsort; 0xded0 createaccount; 0xf4a0 area_createsector ×3; 0x129b0 createbond; 0x13d80 building_load; 0x143b0 createbullet; 0x19ee0 createjointbond; 0x1b070 createitem; 0x278c0 createparticle; 0x28020 createplayer; 0x306d0 human_createmuscleball; 0x31800 createobject; 0x33650 serverrecv; 0x50ae0 createhuman; 0x54e40 createlevel | Clears new records in the `create*` functions (`createplayer`, `createhuman`, `createitem`, `createobject`, `createbond`, `createbullet`, `createparticle`, ...), `createlevel` and `serverrecv`. |
| `HeapSetInformation` | KERNEL32 | 1 | 0x5d08c __tmainCRTStartup | CRT startup: turns on the terminate-on-corruption heap option. |
| `EncodePointer` | KERNEL32 | 2 | 0x5d25c sub_5d25c~; 0x5d492 __onexit | CRT startup (`__tmainCRTStartup`) and `__onexit`: pointer obfuscation for the atexit table. |
| `DecodePointer` | KERNEL32 | 1 | 0x5d492 __onexit | `__onexit` only. |

### 2.2 Strings and text

There is no `strcpy`, `strlen` or `strcmp` import: the compiler inlined them (for example the server name copy in `dedicatedserverloop`, which has no length limit). The only comparator is the 2-byte unrolled string compare at `0xd320` (used by `qsort`).

| Import | DLL | Sites | Called from (RVA, name) | Use |
| --- | --- | ---: | --- | --- |
| `sprintf` | MSVCR100 | 30 | 0x11d0 saveaccountsclient†; 0x12f0 loadaccountsclient†; 0x29a0 loadblock; 0x3740 building_save†; 0x8320 leveleditor_save†; 0x8590 leveleditor_load; 0x8880 sprintf_comma ×4; 0xded0 createaccount; 0x13d80 building_load; 0x185a0 newspaper_load; 0x1ac40 getmasterserverip; 0x1d2c0 logic_farbslist; 0x1d4d0 logic_setteammanager; 0x49940 server_playermessage ×4; 0x4e7a0 loadsaveoptions ×2; 0x58320 scenario_createguards; 0x585b0 scenario_createtraffic2; 0x587f0 scenario_createchase ×4; 0x5b9b0 logicsimulation_world; 0x5c8d0 dedicatedserverloop | Builds file names (`data/block/%s.sbl`, `data/building/%s.sbb`, `data/city.sbc`), chat/log lines (`%s is now manager of %s`, ` Joined`, ` Exited`), `Agent %d` default name, the newspaper top-10 line. |
| `sscanf` | MSVCR100 | 4 | 0xd110 getfloat; 0xd1a0 sub_d1a0~†; 0x1ac40 getmasterserverip; 0x49940 server_playermessage | `getfloat`/`getint` number parsing of `config.txt`, the master-server reply (`%d`), and the chat command parser in `server_playermessage`. |
| `strncmp` | MSVCR100 | 7 | 0x1010 account_nameexists; 0x49940 server_playermessage ×6 | Account name lookup (`account_nameexists`, 24 characters) and the admin command prefixes in `server_playermessage` (`/kick `, `/ban1 `, `/give `...). |
| `tolower` | MSVCR100 | 1 | 0x49940 server_playermessage | Case-insensitive command matching in `server_playermessage`. |
| `qsort` | MSVCR100 | 1 | 0xd3a0 listfiles | `listfiles`: sorts the directory listing with the string compare at RVA 0xd320 (unlisted in `functions.csv`). |
| `printf` | MSVCR100 | 1 | 0x5c8d0 dedicatedserverloop | Three console messages at startup (`dedicatedserverloop`): master-server result, `port:%d`, `socketenabled:%d`. |

### 2.3 Math (x87, `_CI*`) and random numbers

`_CI*` are the "no-stack-frame" x87 intrinsics of `MSVCR100.dll`. The argument is in ST0 (or ST1 and ST0 for `_CIatan2`, `_CIpow`) and the result is in ST0. `tools/ghidra/FixX87Intrinsics.java` and `src/lifted/x87_intrinsics.c` already handle this convention for the hooks (`docs/lift.md`). `rand` and `srand` are the MSVC generator, one 32-bit state per thread, returning 15 bits.

| Import | DLL | Sites | Called from (RVA, name) | Use |
| --- | --- | ---: | --- | --- |
| `_CIsqrt` | MSVCR100 | 137 | 0x9600 geteulerangles2; 0xa7a0 orientationtoquaternion; 0xb8c0 vectorlength; 0xb900 normalizevector; 0xbaa0 vectordistance; 0xbf30 sub_bf30~†; 0xbf60 normalizevector2d; 0xc8f0 pointdistanceline; 0xcdf0 lineintersectcircle2d† ×3; 0xcf90 sub_cf90~†; 0xe4d0 ai_playercar; 0xf6f0 addbeziersurface ×2; 0xfc40 addbeziersurfacetexture ×2; 0x146a0 lineintersectcityobject ×2; 0x14dd0 lineintersectcityobjecttype ×2; 0x15ca0 sub_15ca0~†; 0x1a000 circleintersecttriangle ×4; 0x1a7f0 sphereintersectline ×2; 0x1a990 sphereintersectpoint†; 0x1aa60 lineintersectsphere ×2; 0x1bc50 createlandscape; 0x1d3b0 logicsimulation_driving†; 0x1dba0 unpackobjectpacket† ×3; 0x1f4a0 packobjectpacket2† ×6; 0x20680 packobjectpacket ×8; 0x225e0 getobjectrotation3; 0x240e0 object_rotorsimulation† ×9; 0x24f60 objectsimulation_heli ×3; 0x25e70 createtrack ×7; 0x27e80 calculatefriction ×2; 0x28c90 calculatepointscale ×2; 0x29300 objecttype_calcquadweights; 0x2f130 lineintersecthuman ×2; 0x2f420 human_calcarmtorque ×4; 0x34870 setupobjecttypeweight ×2; 0x3cfe0 calculateplayervoice ×3; 0x3d500 ai_assessthreats ×4; 0x41de0 bondsimulation2 ×11; 0x46c30 building_buildfloorplan; 0x48ec0 bulletsimulation ×4; 0x4a440 human_calculatearmangles; 0x4cee0 item_physicssimulation; 0x4f720 playerai ×2; 0x51060 humansimulation ×12; 0x542a0 itemsimulation ×5; 0x56710 objectsimulation ×10 | Vector length/normalise, distance, collision tests, physics. The most used import: 137 call sites. |
| `_CIsin` | MSVCR100 | 21 | 0x9600 geteulerangles2; 0xbc50 rotatevector; 0xbdb0 rotateorientation; 0xc0a0 rotatevector2d†; 0x19060 human_calculatelegangles ×4; 0x240e0 object_rotorsimulation†; 0x24f60 objectsimulation_heli; 0x272b0 objectsimulation_car ×2; 0x2f420 human_calcarmtorque ×2; 0x32fe0 buildsomeshit ×3; 0x4a440 human_calculatearmangles ×2; 0x51060 humansimulation ×2 | Rotations and orientation (`rotatevector`, `rotateorientation`), humans and vehicles. |
| `_CIcos` | MSVCR100 | 18 | 0x9600 geteulerangles2; 0x9730 sub_9730~†; 0x9be0 cerpnoise2d† ×2; 0xbc50 rotatevector; 0xbdb0 rotateorientation; 0xc0a0 rotatevector2d†; 0x240e0 object_rotorsimulation†; 0x272b0 objectsimulation_car ×2; 0x2f420 human_calcarmtorque ×2; 0x32fe0 buildsomeshit ×3; 0x4a440 human_calculatearmangles ×2; 0x51060 humansimulation | Same families as `_CIsin`, plus `cerpnoise2d`. |
| `_CItan` | MSVCR100 | 1 | 0x272b0 objectsimulation_car | One site: `objectsimulation_car`. |
| `_CIacos` | MSVCR100 | 6 | 0x19060 human_calculatelegangles; 0x2f420 human_calcarmtorque ×4; 0x4a440 human_calculatearmangles | Arm and leg angle solvers (`human_calcarmtorque`, `human_calculatearmangles`, `human_calculatelegangles`). |
| `_CIatan2` | MSVCR100 | 22 | 0x9600 geteulerangles2 ×3; 0x19060 human_calculatelegangles; 0x240e0 object_rotorsimulation† ×3; 0x272b0 objectsimulation_car; 0x4a440 human_calculatearmangles ×5; 0x4f720 playerai ×5; 0x51060 humansimulation ×4 | Euler angles, AI aiming, arm angles, helicopter rotor. |
| `_CIpow` | MSVCR100 | 5 | 0x9690 sub_9690~†; 0x1f4a0 packobjectpacket2† ×2; 0x20680 packobjectpacket ×2 | `packobjectpacket`/`packobjectpacket2` (non-linear quantisation of values for the network) and `geteulerangles2`. |
| `floor` | MSVCR100 | 2 | 0x1c1d0 lineintersectlandscape; 0x2c0c0 lineintersectarea | Landscape and area ray casts (`lineintersectlandscape`, `lineintersectarea`). |
| `rand` | MSVCR100 | 47 | 0x1610 ai_car_nextwaypoint ×3; 0x5700 economy_carmarket; 0xded0 createaccount; 0xe2e0 loadaccountsserver ×3; 0xe4d0 ai_playercar; 0x22e20 applydamageobject; 0x28020 createplayer; 0x28230 createmission_threeway ×6; 0x286b0 createmission_transaction ×2; 0x48ec0 bulletsimulation ×2; 0x4e7a0 loadsaveoptions ×2; 0x4f720 playerai ×6; 0x50ae0 createhuman ×2; 0x55700 logic_playeractions; 0x58320 scenario_createguards ×3; 0x585b0 scenario_createtraffic2 ×4; 0x587f0 scenario_createchase ×2; 0x58ef0 createmission_acquisition ×2; 0x590f0 resetgame ×2; 0x5aa60 createscenario_world; 0x5ae70 logicsimulation_round | MSVC linear congruential generator. Level generation, AI, economy, account look values, damage. |
| `srand` | MSVCR100 | 3 | 0x4e7a0 loadsaveoptions; 0x54e40 createlevel; 0x590f0 resetgame | `createlevel` seeds with 0 (deterministic city), `resetgame` and `loadsaveoptions` seed with the time. |

### 2.4 Files and console

The game reads and writes files through two function pointers, `freadptr` (RVA `0x91022a4`, VA `0x95022a4`) and `fwriteptr` (RVA `0x13bf2d00`). `getpaths` (main) fills them with the addresses of `fread` and `fwrite`. The source has them because the big-endian Mac build points them to byte-swapping versions (`docs/formatos.md`). `fopen`, `fclose`, `fgetc`, `fprintf` and `fseek` are called directly. The `FILE *` of the file in use is kept in a global (`parser+0x100108` for the text files, RVA `0x13e22e68`).

| Import | DLL | Sites | Called from (RVA, name) | Use |
| --- | --- | ---: | --- | --- |
| `fopen` | MSVCR100 | 14 | 0x1080 saveaccountsserver; 0x11d0 saveaccountsclient†; 0x12f0 loadaccountsclient†; 0x1420 server_writelogfile; 0x29a0 loadblock; 0x3740 building_save†; 0x5280 debug_resetlogfile; 0x52f0 sub_52f0~†; 0x8320 leveleditor_save†; 0x8590 leveleditor_load; 0xd020 loadtextfile; 0xe2e0 loadaccountsserver; 0x13d80 building_load; 0x5c8d0 dedicatedserverloop | Opens every game file (see section 2). |
| `fclose` | MSVCR100 | 17 | 0x1080 saveaccountsserver; 0x11d0 saveaccountsclient†; 0x12f0 loadaccountsclient†; 0x1420 server_writelogfile; 0x29a0 loadblock ×2; 0x3740 building_save†; 0x5280 debug_resetlogfile; 0x8320 leveleditor_save†; 0x8590 leveleditor_load ×2; 0xd020 loadtextfile; 0xe2e0 loadaccountsserver; 0x13d80 building_load ×3; 0x5c8d0 dedicatedserverloop | Closes them. |
| `fread` | MSVCR100 | 2 | 0x296a0 sub_296a0~†; 0x5cd10 getpaths | Only its address is used: `getpaths` (main) stores it in the global `freadptr` and all game readers call it through the pointer. |
| `fwrite` | MSVCR100 | 2 | 0x296a0 sub_296a0~†; 0x5cd10 getpaths | Same for `fwriteptr` (writers). |
| `fgetc` | MSVCR100 | 4 | 0xd020 loadtextfile; 0xd4e0 sub_d4e0~† ×3 | Text readers: `loadtextfile` (`config.txt`, `readme.txt`) and the dead directory helpers. |
| `fputc` | MSVCR100 | 3 | 0xd5a0 sub_d5a0~† ×3 | Dead directory helpers only. |
| `fprintf` | MSVCR100 | 5 | 0x1420 server_writelogfile; 0x5280 debug_resetlogfile; 0x52f0 sub_52f0~†; 0x332a0 sub_332a0~†; 0x5c8d0 dedicatedserverloop | `server_writelogfile` (`stats.txt`), `debug_resetlogfile` and the main loop (`dedlog.txt`). |
| `fseek` | MSVCR100 | 2 | 0x52f0 sub_52f0~†; 0x5c8d0 dedicatedserverloop | Seeks to the end before appending a line to `dedlog.txt`. |
| `_findfirst64i32` | MSVCR100 | 1 | 0xd3a0 listfiles | `listfiles`: `data/block/*.sbl` and `data/texture/*.png` listings. |
| `_findnext64i32` | MSVCR100 | 1 | 0xd3a0 listfiles | Same. |
| `_findclose` | MSVCR100 | 1 | 0xd3a0 listfiles | Same. |
| `_chdir` | MSVCR100 | 20 | 0x11d0 saveaccountsclient† ×2; 0x12f0 loadaccountsclient† ×2; 0x2a510 area_createinteriorblocks ×4; 0x3e070 area_createsblocks ×4; 0x4e7a0 loadsaveoptions ×4; 0x502a0 setupgame ×4 | Changes the working directory around the directory listings and back to the start directory (`loadsaveoptions`, `setupgame`, `area_createsblocks`, `area_createinteriorblocks`). The two client account functions are dead. |
| `_getcwd` | MSVCR100 | 4 | 0xde90 sub_de90~† ×2; 0x5cd10 getpaths ×2 | `getpaths` (main) saves the start directory twice (two 512-byte buffers). |

### 2.5 Time

Only one clock is used for the simulation: `QueryPerformanceCounter` scaled to milliseconds with a `double` factor made by `getcpufrequency` (RVA `0x916d6f0`). The loop waits with `_sleep(1)`. Wall-clock time is used only as a random seed. `RDTSC` is also used 24 times by the inline profiler (`starttimer`/`stoptimer`, array `timer`), not through an import.

| Import | DLL | Sites | Called from (RVA, name) | Use |
| --- | --- | ---: | --- | --- |
| `_time64` | MSVCR100 | 4 | 0x1000 sub_1000~†; 0x4e7a0 loadsaveoptions; 0x590f0 resetgame; 0x5c8d0 dedicatedserverloop | Seeds `srand` and `init_genrand` (`loadsaveoptions`, `resetgame`, `dedicatedserverloop`). |
| `QueryPerformanceCounter` | KERNEL32 | 4 | 0x5350 systemtimer†; 0x5c8d0 dedicatedserverloop ×2; 0x5d7f0 __security_init_cookie | Main-loop frame pacing in `dedicatedserverloop` (2 sites) and the CRT cookie seed. `systemtimer` (dead) also uses it. |
| `QueryPerformanceFrequency` | KERNEL32 | 1 | 0xb4a0 getcpufrequency | `getcpufrequency`: computes the factor that turns counter ticks into milliseconds. |
| `_sleep` | MSVCR100 | 2 | 0x1ac40 getmasterserverip; 0x5c8d0 dedicatedserverloop | `dedicatedserverloop`: `_sleep(1)` between polls. `getmasterserverip`: `_sleep(100)` in the connect and receive loops. |
| `Sleep` | KERNEL32 | 1 | 0x5d08c __tmainCRTStartup | CRT startup lock spin only. |
| `GetTickCount` | KERNEL32 | 1 | 0x5d7f0 __security_init_cookie | CRT cookie seed only. |
| `GetSystemTimeAsFileTime` | KERNEL32 | 2 | 0xb230 gettimeofday†; 0x5d7f0 __security_init_cookie | CRT cookie seed. `gettimeofday` (dead) also. |
| `__daylight` | MSVCR100 | 1 | 0xb230 gettimeofday† | `gettimeofday` (dead). |
| `__timezone` | MSVCR100 | 1 | 0xb230 gettimeofday† | `gettimeofday` (dead). |
| `_tzset` | MSVCR100 | 1 | 0xb230 gettimeofday† | `gettimeofday` (dead). |

### 2.6 Network (WinSock 1.1, `WSOCK32.dll`)

All 21 imports are by ordinal and are `stdcall`. `hook/wsock32.def` lists the names. The server uses **one UDP socket** (created in `0xd710`, port from `config.txt`, non-blocking) for all game traffic, and **one short-lived TCP socket** at start-up for the HTTP request to the master server. It has no `listen`/`accept`, no `WSAAsync*` and no `WSACleanup`. The wire format is in `docs/protocol.md`. In memory, the socket state is the global `bsdsocket` (`0x20080` bytes): socket handles as 32-bit ints at `+0x24`, flags at `+0x4`, a `sockaddr_in` at `+0x44`, the port at `+0x58`, the 64 KiB send buffer at `+0x64` (it starts with the 4-byte magic `7DFP`) and the 64 KiB receive buffer at `+0x10070`.

Dead code that also uses WinSock: `0xd7b0` (client socket, no bind), `0xd840` (the real `disablesocket`, closes the socket), `0x6a50` (lists the local IP addresses with `gethostname`/`gethostbyname`).

| Import | Ordinal | Sites | Called from (RVA, name) | Use |
| --- | ---: | ---: | --- | --- |
| `WSAStartup` | 115 | 1 | 0x5cd10 getpaths | `getpaths` (main): `WSAStartup(0x101, &wsadata)`. |
| `socket` | 23 | 3 | 0xd710 disablesocket; 0xd7b0 sub_d7b0~†; 0x1ac40 getmasterserverip | `disablesocket`@0xd710 (really `enablesocketserver`, see section 8): UDP socket. `getmasterserverip`: TCP socket. |
| `bind` | 2 | 1 | 0xd710 disablesocket | Binds the UDP socket to the server port. |
| `ioctlsocket` | 12 | 3 | 0xd710 disablesocket; 0xd7b0 sub_d7b0~†; 0x1ac40 getmasterserverip | `FIONBIO` (0x8004667e): non-blocking mode. |
| `closesocket` | 3 | 2 | 0xd840 sub_d840~†; 0x1ac40 getmasterserverip | `getmasterserverip` after the HTTP reply. The real `disablesocket` (0xd840) is dead. |
| `sendto` | 20 | 4 | 0xd910 sendpacket; 0x33650 serverrecv; 0x4ed30 serversend ×2 | `sendpacket`, `serversend`, `serverrecv`: all game traffic (UDP). |
| `recvfrom` | 17 | 1 | 0xd860 recvpacket | `recvpacket` (called by `serverrecv`): all game traffic in. |
| `htons` | 9 | 7 | 0xd710 disablesocket; 0xd7b0 sub_d7b0~†; 0xd910 sendpacket; 0x1ac40 getmasterserverip; 0x33650 serverrecv; 0x4ed30 serversend ×2 | Port in `disablesocket`@0xd710, `sendpacket`, `serversend`, `serverrecv`, `getmasterserverip`. |
| `htonl` | 8 | 4 | 0xd910 sendpacket; 0x33650 serverrecv; 0x4ed30 serversend ×2 | Address handling in `sendpacket`, `serversend`, `serverrecv`. |
| `ntohl` | 14 | 9 | 0x6a50 sub_6a50~†; 0x1ac40 getmasterserverip ×2; 0x33650 serverrecv ×6 | `serverrecv` (client address), `getmasterserverip` (master address). |
| `ntohs` | 15 | 5 | 0x33650 serverrecv ×5 | `serverrecv` (client port). |
| `inet_addr` | 10 | 2 | 0x1ac40 getmasterserverip ×2 | `getmasterserverip`: fallback master address and the address text from the master reply. |
| `connect` | 4 | 1 | 0x1ac40 getmasterserverip | `getmasterserverip`: TCP connect to www.crypticsea.com:80. |
| `send` | 19 | 1 | 0x1ac40 getmasterserverip | `getmasterserverip`: the HTTP GET. |
| `recv` | 16 | 1 | 0x1ac40 getmasterserverip | `getmasterserverip`: the HTTP reply (up to 8 KB). |
| `select` | 18 | 1 | 0x1ac40 getmasterserverip | `getmasterserverip`: connect/receive wait. |
| `__WSAFDIsSet` | 151 | 2 | 0x1ac40 getmasterserverip ×2 | `getmasterserverip`: FD_ISSET. |
| `getsockopt` | 7 | 1 | 0x1ac40 getmasterserverip | `getmasterserverip`: `SO_ERROR` after the non-blocking connect. |
| `gethostbyname` | 52 | 2 | 0x6a50 sub_6a50~†; 0x1ac40 getmasterserverip | `getmasterserverip`: resolves `www.crypticsea.com`. |
| `getservbyname` | 55 | 1 | 0x1ac40 getmasterserverip | `getmasterserverip`: service `http`/`tcp`. |
| `gethostname` | 57 | 1 | 0x6a50 sub_6a50~† | Only the dead function 0x6a50 (unlisted). |

### 2.7 Process, CRT start-up and crash handling

`KERNEL32` calls here are almost all inside the MSVC runtime that is linked into the `.exe` (section 6). The only ones in game code are `QueryPerformanceCounter`/`Frequency`, `GetCurrentProcess` + `Get/SetProcessAffinityMask` (in the dead `setcpuaffinity`). `MSVCR100.dll` is loaded as a normal DLL. `_controlfp_s` is called by `0x5d7b5` with `_PC_53` / `_MCW_PC`, which sets the 53-bit x87 precision for the whole run.

| Import | DLL | Sites | Called from (RVA, name) | Use |
| --- | --- | ---: | --- | --- |
| `__getmainargs` | MSVCR100 | 1 | 0x5d041 sub_5d041~ | `__tmainCRTStartup`: argc/argv/envp. |
| `_initterm_e` | MSVCR100 | 1 | 0x5d08c __tmainCRTStartup | `__tmainCRTStartup`: runs the pre-C initialisers. |
| `_initterm` | MSVCR100 | 1 | 0x5d08c __tmainCRTStartup | Runs the C initialisers (none are registered by the game). |
| `__set_app_type` | MSVCR100 | 1 | 0x5d25c sub_5d25c~ | Console application. |
| `_controlfp_s` | MSVCR100 | 1 | 0x5d7b5 sub_5d7b5~ | `_setdefaultprecision`: sets the x87 precision control to 53 bits (`_controlfp_s(0, 0x10000, 0x30000)`) at RVA 0x5d7b5. |
| `__setusermatherr` | MSVCR100 | 1 | 0x5d25c sub_5d25c~ | Registers the default `_matherr` hook. |
| `_commode` | MSVCR100 | 1 | 0x5d25c sub_5d25c~ | CRT file commit mode global. |
| `_fmode` | MSVCR100 | 1 | 0x5d25c sub_5d25c~ | CRT default file translation mode global. |
| `__initenv` | MSVCR100 | 1 | 0x5d08c __tmainCRTStartup | CRT environment pointer global. |
| `_configthreadlocale` | MSVCR100 | 1 | 0x5d25c sub_5d25c~ | Locale configuration at startup. |
| `exit` | MSVCR100 | 1 | 0x5d08c __tmainCRTStartup | Exit with the code returned by main. |
| `_exit` | MSVCR100 | 1 | 0x5d08c __tmainCRTStartup | Fast exit path. |
| `_cexit` | MSVCR100 | 1 | 0x5d08c __tmainCRTStartup | Cleanup exit path. |
| `_amsg_exit` | MSVCR100 | 2 | 0x5d041 sub_5d041~; 0x5d08c __tmainCRTStartup | Fatal CRT error path. |
| `_XcptFilter` | MSVCR100 | 1 | 0x5d08c __tmainCRTStartup | Exception filter in `mainCRTStartup`. |
| `_crt_debugger_hook` | MSVCR100 | 2 | 0x5d320 __report_gsfailure ×2 | CRT debugger hook (thunk). |
| `?terminate@@YAXXZ` | MSVCR100 | 1 | 0x5d436 __CxxUnhandledExceptionFilter | C++ `terminate` (thunk; `__CxxUnhandledExceptionFilter`, dead). |
| `_lock` | MSVCR100 | 1 | 0x5d492 __onexit | CRT lock (thunk). |
| `_unlock` | MSVCR100 | 1 | 0x5d52a _unlockexit | CRT unlock (thunk). |
| `_onexit` | MSVCR100 | 1 | 0x5d492 __onexit | `atexit` table (dead). |
| `__dllonexit` | MSVCR100 | 1 | 0x5d492 __onexit | `atexit` table (dead). |
| `_except_handler4_common` | MSVCR100 | 1 | 0x5d769 __except_handler4 | SEH4 handler backend (`__except_handler4`). |
| `_invoke_watson` | MSVCR100 | 2 | 0x5d78e sub_5d78e~†; 0x5d7b5 sub_5d7b5~ | Fatal CRT error report. |
| `SetProcessAffinityMask` | KERNEL32 | 1 | 0xb300 setcpuaffinity† | `setcpuaffinity` (dead). The config key `setcpuaffinity=` is read but nothing calls the function. |
| `GetProcessAffinityMask` | KERNEL32 | 1 | 0xb300 setcpuaffinity† | `setcpuaffinity` (dead). |
| `GetCurrentProcess` | KERNEL32 | 2 | 0xb300 setcpuaffinity†; 0x5d320 __report_gsfailure | `__report_gsfailure`, `setcpuaffinity` (dead). |
| `GetCurrentProcessId` | KERNEL32 | 1 | 0x5d7f0 __security_init_cookie | Cookie seed. |
| `GetCurrentThreadId` | KERNEL32 | 1 | 0x5d7f0 __security_init_cookie | Cookie seed. The server has no threads. |
| `IsProcessorFeaturePresent` | KERNEL32 | 1 | 0x5d426 sub_5d426~ | `__report_gsfailure`. |
| `IsDebuggerPresent` | KERNEL32 | 1 | 0x5d320 __report_gsfailure | `__report_gsfailure`. |
| `SetUnhandledExceptionFilter` | KERNEL32 | 2 | 0x5d320 __report_gsfailure; 0x5d478 sub_5d478~ | `__report_gsfailure`, `__CxxUnhandledExceptionFilter` (dead). |
| `UnhandledExceptionFilter` | KERNEL32 | 1 | 0x5d320 __report_gsfailure | `__report_gsfailure`. |
| `TerminateProcess` | KERNEL32 | 1 | 0x5d320 __report_gsfailure | `__report_gsfailure`. |
| `InterlockedCompareExchange` | KERNEL32 | 1 | 0x5d08c __tmainCRTStartup | `__tmainCRTStartup`: startup lock. |
| `InterlockedExchange` | KERNEL32 | 1 | 0x5d08c __tmainCRTStartup | `__tmainCRTStartup`: startup unlock. |

