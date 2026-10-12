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
| 5 | Ghidra headless decompile (`tools/ghidra/DecompileFunctions.java`) | Used to confirm `getmasterserverip`, `server_writelogfile`, `loadsaveoptions`, `saveaccountsserver`, `loadaccountsserver`, `leveleditor_load`, `newspaper_load`, `servercycle`, `sendpacket`, `recvpacket`. |
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
- **`rand()` is used by 21 functions (47 sites) and is seeded from the clock.** `createlevel` calls `srand(0)`, but no function below it calls `rand`, so the level does not depend on the generator. All uses are bit masks or `% n` with a small `n`, none scales by `RAND_MAX` (section 7, R2).
- **`functions.csv` covers the live code except 7 functions** (6 in the CRT and the `qsort` comparator at `0xd320`). 32 of its 340 functions are not referenced by live code (client-only or editor code).
- The function `getpaths` (`0x5cd10`) is the real `main`, and `disablesocket` (`0xd710`) really opens the server socket. See the naming notes in section 8.

## 2. Imports

The 024d calls most imports with `call [IAT]`. Some of them are reached only through a `jmp [IAT]` thunk (`memcpy`, `memset`, `_CI*`, all WinSock functions). The thunks are at `0x5cd7c`–`0x5ce17` and `0x5d88c`–`0x5d93a`. The tables count both ways.

### 2.1 Memory

The game code allocates from the heap in two functions only. `createarea` (`0xf2b0`) frees and allocates one block per level area (4 areas, record size `0x100d6c`), and `landscape_buildsectorlods` (`0x1e50`) does the same for 4 buffers in each `area_block` record (record size `0x84`; the name says landscape, but the address is the `area_block` array of section 4). In both cases the pointer is stored **inside a global struct** (see section 7, R3). `memcpy` and `memset` come from `MSVCR100` through thunks. `memset` clears each new record in the `create*` functions with the size of the struct.

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
| `sprintf` | MSVCR100 | 30 | 0x11d0 saveaccountsclient†; 0x12f0 loadaccountsclient†; 0x29a0 loadblock; 0x3740 building_save†; 0x8320 leveleditor_save†; 0x8590 leveleditor_load; 0x8880 sprintf_comma ×4; 0xded0 createaccount; 0x13d80 building_load; 0x185a0 newspaper_load; 0x1ac40 getmasterserverip; 0x1d2c0 logic_farbslist; 0x1d4d0 logic_setteammanager; 0x49940 server_playermessage ×4; 0x4e7a0 loadsaveoptions ×2; 0x58320 scenario_createguards; 0x585b0 scenario_createtraffic2; 0x587f0 scenario_createchase ×4; 0x5b9b0 logicsimulation_world; 0x5c8d0 dedicatedserverloop | Builds file names (`data/block/%s.sbl`, `data/building/%s.sbb`, `data/city.sbc`), chat and mission text (`logic_setteammanager`: `%s is now manager of %s`; `server_playermessage`; `scenario_*`), the `Agent %d` default name, the `physics:%d` debug line, the newspaper top-10 line (`%d. %s   $`) and the thousands separators of `sprintf_comma`. |
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
| `rand` | MSVCR100 | 47 | 0x1610 ai_car_nextwaypoint ×3; 0x5700 economy_carmarket; 0xded0 createaccount; 0xe2e0 loadaccountsserver ×3; 0xe4d0 ai_playercar; 0x22e20 applydamageobject; 0x28020 createplayer; 0x28230 createmission_threeway ×6; 0x286b0 createmission_transaction ×2; 0x48ec0 bulletsimulation ×2; 0x4e7a0 loadsaveoptions ×2; 0x4f720 playerai ×6; 0x50ae0 createhuman ×2; 0x55700 logic_playeractions; 0x58320 scenario_createguards ×3; 0x585b0 scenario_createtraffic2 ×4; 0x587f0 scenario_createchase ×2; 0x58ef0 createmission_acquisition ×2; 0x590f0 resetgame ×2; 0x5aa60 createscenario_world; 0x5ae70 logicsimulation_round | MSVC linear congruential generator (15 bits). AI, economy, scenarios, spawn choices, damage, account look values. All uses are `& mask` or `% n`. |
| `srand` | MSVCR100 | 3 | 0x4e7a0 loadsaveoptions; 0x54e40 createlevel; 0x590f0 resetgame | `createlevel` seeds with 0 (nothing below it calls `rand`), `resetgame` and `loadsaveoptions` seed with the time. |

### 2.4 Files and console

The game reads and writes files through two function pointers, `freadptr` (RVA `0x91022a4`, VA `0x95022a4`) and `fwriteptr` (RVA `0x13bf2d00`). `getpaths` (main) fills them with the addresses of `fread` and `fwrite`. The source has them because the big-endian Mac build points them to byte-swapping versions (`docs/formatos.md`). `fopen`, `fclose`, `fgetc`, `fprintf` and `fseek` are called directly. The `FILE *` of the file in use is kept in a global (`parser+0x100108` for the text files, RVA `0x13e22e68`).

| Import | DLL | Sites | Called from (RVA, name) | Use |
| --- | --- | ---: | --- | --- |
| `fopen` | MSVCR100 | 14 | 0x1080 saveaccountsserver; 0x11d0 saveaccountsclient†; 0x12f0 loadaccountsclient†; 0x1420 server_writelogfile; 0x29a0 loadblock; 0x3740 building_save†; 0x5280 debug_resetlogfile; 0x52f0 sub_52f0~†; 0x8320 leveleditor_save†; 0x8590 leveleditor_load; 0xd020 loadtextfile; 0xe2e0 loadaccountsserver; 0x13d80 building_load; 0x5c8d0 dedicatedserverloop | Opens every game file (see section 3). |
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
| `__getmainargs` | MSVCR100 | 1 | 0x5d041 sub_5d041~ | The `_initterm` initialiser `0x5d041`: argc/argv/envp. |
| `_initterm_e` | MSVCR100 | 1 | 0x5d08c __tmainCRTStartup | `__tmainCRTStartup`: runs the pre-C initialisers. |
| `_initterm` | MSVCR100 | 1 | 0x5d08c __tmainCRTStartup | Runs the C initialiser table (one entry, `0x5d041`). |
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
| `_XcptFilter` | MSVCR100 | 1 | 0x5d08c __tmainCRTStartup | SEH exception filter of `__tmainCRTStartup`. |
| `_crt_debugger_hook` | MSVCR100 | 2 | 0x5d320 __report_gsfailure ×2 | CRT debugger hook (thunk). |
| `?terminate@@YAXXZ` | MSVCR100 | 1 | 0x5d436 __CxxUnhandledExceptionFilter | C++ `terminate`, called by `__CxxUnhandledExceptionFilter` (it runs only on an unhandled C++ exception, which the game cannot raise). |
| `_lock` | MSVCR100 | 1 | 0x5d492 __onexit | CRT lock (thunk). |
| `_unlock` | MSVCR100 | 1 | 0x5d52a _unlockexit | CRT unlock (thunk). |
| `_onexit` | MSVCR100 | 1 | 0x5d492 __onexit | `atexit` table: registers the one exit function `0x5d570`. |
| `__dllonexit` | MSVCR100 | 1 | 0x5d492 __onexit | `atexit` table. |
| `_except_handler4_common` | MSVCR100 | 1 | 0x5d769 __except_handler4 | SEH4 handler backend (`__except_handler4`). |
| `_invoke_watson` | MSVCR100 | 2 | 0x5d78e sub_5d78e~†; 0x5d7b5 sub_5d7b5~ | Fatal CRT error report. |
| `SetProcessAffinityMask` | KERNEL32 | 1 | 0xb300 setcpuaffinity† | `setcpuaffinity` (dead). The config key `setcpuaffinity=` is read but nothing calls the function. |
| `GetProcessAffinityMask` | KERNEL32 | 1 | 0xb300 setcpuaffinity† | `setcpuaffinity` (dead). |
| `GetCurrentProcess` | KERNEL32 | 2 | 0xb300 setcpuaffinity†; 0x5d320 __report_gsfailure | `__report_gsfailure`, `setcpuaffinity` (dead). |
| `GetCurrentProcessId` | KERNEL32 | 1 | 0x5d7f0 __security_init_cookie | Cookie seed. |
| `GetCurrentThreadId` | KERNEL32 | 1 | 0x5d7f0 __security_init_cookie | Cookie seed. The server has no threads. |
| `IsProcessorFeaturePresent` | KERNEL32 | 1 | 0x5d426 sub_5d426~ | `IsProcessorFeaturePresent(10)`: the SSE2 flag that `_ftol2` reads. |
| `IsDebuggerPresent` | KERNEL32 | 1 | 0x5d320 __report_gsfailure | `__report_gsfailure`. |
| `SetUnhandledExceptionFilter` | KERNEL32 | 2 | 0x5d320 __report_gsfailure; 0x5d478 sub_5d478~ | `__report_gsfailure` (removes the filter) and the initialiser `0x5d478` (installs `__CxxUnhandledExceptionFilter` at start-up). |
| `UnhandledExceptionFilter` | KERNEL32 | 1 | 0x5d320 __report_gsfailure | `__report_gsfailure`. |
| `TerminateProcess` | KERNEL32 | 1 | 0x5d320 __report_gsfailure | `__report_gsfailure`. |
| `InterlockedCompareExchange` | KERNEL32 | 1 | 0x5d08c __tmainCRTStartup | `__tmainCRTStartup`: startup lock. |
| `InterlockedExchange` | KERNEL32 | 1 | 0x5d08c __tmainCRTStartup | `__tmainCRTStartup`: startup unlock. |

## 3. Files

The server uses **relative paths** and changes directory with `_chdir`. It must start in a folder that has `config.txt`, `data/` and `user/`. `getpaths` (main) saves the start directory twice with `_getcwd`; `loadsaveoptions` and the directory scans return to it.

Formats are in `docs/formatos.md`. Readers and writers are in `tools/formats/subrosa_formats.py`. The `config.txt` of a real server has passwords: this document lists the **keys** only.

### 3.1 Files that the live code opens

| File | Mode | Opened by (RVA, name) | When | Format |
| --- | --- | --- | --- | --- |
| `config.txt` | `rb` | `0xd020 loadtextfile`, called by `0x4e7a0 loadsaveoptions` | Start-up, once | Text. `loadtextfile` reads the whole file, byte by byte with `fgetc`, into the global `parser` (1 MiB, **no size check**). Then `0x29650` (named `optionreadwriteint`, it is a key search) finds `key=` and `0xd110` (`getfloat`) or `0xd240` (`getstring`) reads the value. Keys: `screenwidth`, `screenheight`, `colordepth`, `depthbits`, `stencilbits`, `fullscreen`, `joystick`, `sound`, `soundvolume`, `musicvolume`, `port`, `servermaxbytespersecond`, `name`, `servername`, `serverpassword`, `gametype`, `invertmouse`, `maxnumofplayers`, `roundtime`, `setcpuaffinity`, `voicechat`, `voiceholdtotalk`, `voicekey`, `voicemb`, `voicemin`, `voiceboost`, `help`, `occlusion`. The server uses `port`, `servermaxbytespersecond`, `servername`, `serverpassword`, `gametype`, `maxnumofplayers`, `roundtime`. The other keys are stored in `option` (RVA `0x90dc340`) or in the video settings next to `numofbullets` (RVA `0x17529a60`), and nothing in the server reads them again. A missing file is not an error: the defaults set by `loadsaveoptions` stay. |
| `readme.txt` | `rb` | `0xd020 loadtextfile`, called by `0x185a0 newspaper_load` (from `resetgame`) | At each round start | Optional text, one newspaper line per line. Up to 1022 lines of 124 characters. `newspaper_load` adds a `Top 10 list` and 10 lines `%d. %s   $<money>` for the richest accounts. Not present in the Black Houser folder. |
| `user/server.srk` | `rb` | `0xe2e0 loadaccountsserver` | Start-up, once | Binary accounts. Version 1 to 3. The record count is read from the file and **not checked** against the 65536 limit. See `docs/formatos.md`. |
| `user/server.srk` | `wb` | `0x1080 saveaccountsserver`, called by `0x5b9b0 logicsimulation_world` and `0x590f0 resetgame` | Round start (`resetgame`); when the world clock reaches `0x3b5380` (after `player_calcassets` for every player); and in the periodic block of `logicsimulation_world`: every 18,000 units of the world clock (RVA `0x2561d740`, unnamed, it advances by 10 per tick), so about every 1,800 ticks, together with the economy update | Same format, always version 3. `tools/formats` writes identical files. |
| `data/city.sbc` | `rb` | `0x8590 leveleditor_load` | Start-up, once | Binary city (version 6 only): intersections (12 bytes), streets (52 bytes), a 1024 × 1024 tile map, buildings (80 bytes read, 84 bytes in memory). Counts are not checked against the array sizes. It goes to the global `city`. |
| `data/building/<name>.sbb` | `rb` | `0x13d80 building_load`, called by `0x54e40 createlevel` | Each `resetgame` | Binary building (version 6 to 10). The names come from `city.sbc`. `createlevel` also holds the names `aoffice1`, `hospital0`, `gunshop`, `dealership2`, `base1` to `base5`. `setupbuildingtypes` holds `glassonly`, `test2`. |
| `data/block/<name>.sbl` | `rb` | `0x29a0 loadblock`, called by `0x2a510 area_createinteriorblocks` (from `area_createsblocks`, from `setupgame`) | Start-up, once | Binary block (version 1 or 2). `area_createinteriorblocks` loads every file that `listfiles` finds in `data/block`. It also looks up the names `stairs`, `stairsceiling`, `stairsfloor`, `stairsbalcony`, `railing`, `railingrightwall`, `ceilingedge`, `railingleftwall`, `ceilingedgerightwall`, `garagedoor` in that list. |
| `stats.txt` | `w` (text) | `0x1420 server_writelogfile`, called by `0x5b9b0 logicsimulation_world` | In the same periodic block | Text, lines end with `\r\n`: `numofmissions=`, `numofbullets=`, `users=`, `totaltime=`, `averagetime=`, an empty line, then one line `index:name:value` for each account. `value` and `totaltime` come from the account field `+0x30` divided by 10 (the field that `docs/formatos.md` lists as unknown; it looks like a play-time counter). The file is opened in text mode with explicit `\r\n`, so on Windows each line ends with CR CR LF. |

### 3.2 Directory scans (names only)

| Directory | Pattern | Function | Use |
| --- | --- | --- | --- |
| `data/block` | `*.sbl` | `0xd3a0 listfiles` (from `area_createinteriorblocks`, `area_createsblocks`) | The list of block names. `_findfirst64i32` / `_findnext64i32` / `_findclose`, then `qsort` with the comparator at `0xd320`. |
| `data/texture` | `*.png` | `0xd3a0 listfiles` (from `setupgame`) | Texture names, with `removeflieextentions`. The PNG files are **never opened**: the server only needs the names to number the textures. |

### 3.3 Files that exist in the code but are never used by the server

| File | Function | Why it is dead |
| --- | --- | --- |
| `client.srk` | `0x11d0 saveaccountsclient`, `0x12f0 loadaccountsclient` | No caller. Client account list. |
| `data/building/<name>.sbb` (write) | `0x3740 building_save` | No caller. Editor. |
| `data/city.sbc` (write) | `0x8320 leveleditor_save` | No caller. Editor. |
| `dedlog.txt` | `0x5280 debug_resetlogfile`, the `0x52f0` helper (unlisted), and the main loop (`0x5c8d0`, every 64 ticks) | All three test the global at RVA `0x204020`. The program **never writes** that global (7 reads, 0 writes), so the log is off unless someone patches memory. If it were on, the loop would append `physics:%d` (a profiler value) with mode `rb+`. |
| `.csm` models, `data/weapon/` | none | The string `CSMo` is in `.rdata` but no code uses it. The 024d has no model loader. |

### 3.4 What Phase 4 must keep

- The same folder contract (`config.txt`, `data/block`, `data/building`, `data/city.sbc`, `data/texture` for the names, `user/server.srk`).
- The binary readers must give the same in-memory result as `fread` through `freadptr`. All integers are little-endian 32-bit. The structs read from files are fixed-size (the account record is 60 bytes in the file and `0x4c` in memory).
- Hardening (Phase 5): the three binary readers and the text reader trust sizes and counts from the file.

## 4. Global state (`.data`)

`.data` is `0x25e997e0` bytes (606.6 MiB, about 636 MB) from RVA `0x60000` to `0x25ef97e0`. Only the first `0xe00` bytes have a value in the file (the strings `Sub Rosa`, the packet magic `7DFP` at `0x60d08`, the stack cookie at `0x60d18`, and a few constants). The rest is zero memory.

Sources: `analysis/024d/globals.csv`, `analysis/024d/manual_globals.csv`, `src/include/sr_global_table.h` (element sizes), `docs/structs.md` (capacities), and the 0.24c Linux server symbol sizes (`original/vanilla/0.24c_Dedicated_Linux`). Size = element size × capacity. MiB = 2^20 bytes.

**Phase 4 column.** `docs/plano.md` says the static pools must become dynamic allocation with configurable limits (Phase 4, item 2). Marks:

- **pool**: array of game entities with a capacity constant (`SR_MAX_*` in `src/include/sr_globals_024d.h`). To become dynamic, with a configurable limit.
- **world**: level data with a size fixed by the format or the level. To move to the heap, with a fixed size.
- **keep**: small, can stay static.

### 4.1 Named globals, largest first

| RVA | Name | Size | Layout | What it holds | Phase 4 |
| --- | --- | ---: | --- | --- | --- |
| `0x5592600` | `object` | 32.5 MiB (`0x208c000`) | 1024 × `0x8230` | Vehicles and other physical objects (cars, helicopters, train) with their bodies | pool |
| `0x15e39020` | `connection` | 15.1 MiB (`0xf17000`) | 256 × `0xf170` | State of each connected client: event acknowledgement, send budget, packet state | pool |
| `0xe299220` | `city` | 10.0 MiB (`0xa0b9a0`) | one block, size from the ELF symbol | City from `city.sbc`: intersections, streets, 1 MiB tile map, building list, floor plans | world |
| `0x12a81d00` | `speex` | 9.2 MiB (`0x931000`) | one block | Voice codec state (the name is the ELF symbol; the votes in `globals.csv` put it here). The server relays voice frames without decoding them, so most of it is probably unused (not checked). The voice ring that `serverrecv` writes is inside each `player` record, not here | world, likely droppable |
| `0x93dd880` | `event` | 8.0 MiB (`0x800000`) | 65536 × `0x80` | Ring buffer of network events. `numofevents` (RVA `0xf437318`) is the index. Clients acknowledge with 16 bits, so the size is part of the protocol | pool, but keep 65536 (R4) |
| `0x133b2d00` | `area_block` | 8.25 MiB (`0x840000`) | 65536 × `0x84` | Geometry of one `.sbl` block per record. Each record holds 4 heap pointers (section 2.1). Found with `map_globals.py --min-votes 2`; not yet in `globals.csv` | world, **has pointers** |
| `0x15cbf80` | `human` | 5.4 MiB (`0x56c800`) | 512 × `0x2b64` | People: 15 bones, inventory, health | pool |
| `0xf437b60` | `account` | 4.75 MiB (`0x4c0008`) | 8 bytes (key, count) + 65536 × `0x4c` | Account list loaded from `server.srk`. The limit comes from the file format (count ≤ `0xffff`) | pool |
| `0xac51bc0` | `objecttype` | 4.1 MiB (`0x414600`) | 128 × `0x828c` | Vehicle type tables, filled by `setupobjecttypes_*` at start-up | pool |
| `0x17179a40` | `bond` | 3.7 MiB (`0x3b0000`) | 16384 × `0xec` | Physics joints between particles. Counter `numofbonds` (RVA `0x17179a28`) | pool |
| `0xb3e7200` | `particle` | 3.75 MiB (`0x3c0000`) | 4096 × `0x3c0` (size from the ELF) | Physics points | pool |
| `0xededd00` | `player` | 2.25 MiB (`0x240800`) | 256 × `0x2408` | Player slots. `config.txt` `maxnumofplayers` is clamped to 2…128, so half of the pool is never used | pool |
| `0x1409ba0` | `bullet` | 1.4 MiB (`0x170000`) | 16384 × `0x5c` | Bullets. Counter `numofbullets` (RVA `0x17529a40`) | pool |
| `0x13d22d60` | `parser` | 1.0 MiB (`0x100108`) | cursor (`+0`), length (`+4`), bytes (`+8`), `FILE *` (`+0x100108`) | Text file buffer for `loadtextfile` (no size check) | keep (fix the check) |
| `0xe219200` | `filelist` | 0.5 MiB (`0x80020`) | one block | Name lists made by `listfiles` | keep |
| `0x91022c0` | `item` | 0.42 MiB (`0x6b000`) | 1024 × `0x1ac` | Items on the ground and in inventories | pool |
| `0x1579ba0` | `debug` | 0.32 MiB (`0x523e0`) | one block | Debug log buffers: path of `dedlog.txt` at `+0x52384`, its `FILE *` at `+0x523c4`. Off in the 024d (section 3.3) | keep or drop |
| `0x90dc540` | `game` | 0.15 MiB (`0x25d64`) | one struct | Round and world state: state `+0x20`, timing values set at start-up (`+0x28` = 3600, `+0x2c` = `roundtime` × 3600), tick `+0x38` (`ticks`, RVA `0x90dc578`) | keep |
| `0x1952d680` | `bsdsocket` | 0.13 MiB (`0x20080`) | one struct | Socket handles, `sockaddr_in`, 64 KiB send and receive buffers (section 2.6) | keep |
| `0x10357400` | `itemtype` | 26 KiB (`0x6600`) | 64 × `0x198` | Item type table | pool |
| `0x14043a0` | `timer` | 22 KiB (`0x5800`) | 256 × `0x58` | Inline profiler timers (`RDTSC`). `manual_globals.csv` has `0x14043e0`, which is `+0x40` inside the first record | keep or drop |
| `0xf270280` | `body` | 2 KiB (`0x800`) | 32 × `0x40` | Table of 32 body parts of 16 ints each (read by `human_calculatearmangles`) | pool |
| `0x90dc340` | `option` | 352 B (`0x160`) | one struct | Settings from `config.txt` (port `+0`, max bytes per second `+4`, name `+8`, ...). New: found with `--min-votes 2` | keep |
| `0x916d700` | `soundlist` | 320 B | one block | Sound names | keep |
| `0xf26fd00` | `materiallist` | 1.4 KiB | one block | Material table | keep |
| `0x761ea60` | `server` | 160 B | one struct | Server state: running flag `+0`, name, max players `+0x5c` | keep |
| `0x12a81c60` | `timerlist` | 96 B | one block | Profiler timer indices | keep |
| `0x1b49788` | `stats` | 32 B | one block | Counters written to `stats.txt` (`numofbullets`, `numofmissions`) | keep |
| `0x90dc520` | `numoftracks` | 20 B | one block | Track counters (ELF symbol `numoftracks`) | keep |
| `0x103484a0` | `intersect` | small | one block | Intersection test result record (`black_market.c` of the server manager reads `+0` and `+0xc` as positions) | keep |
| `0x60170`, `0x60174` | `cubequad`, `cubevertex` | `0x60` each | constants | Unit cube tables (initialised data) | keep |

Not named in the CSVs, but of fixed meaning (for Phase 4 they matter as much as the named ones):

| RVA | Name | Size | What it holds | Evidence |
| --- | --- | ---: | --- | --- |
| `0x916d2e0`, `0x916d4e0` | start directory, `config` directory | 2 × 512 B | The two `_getcwd` results | `getpaths` |
| `0x91022a4`, `0x13bf2d00` | `freadptr`, `fwriteptr` | 4 B each | Pointers to `fread`, `fwrite` | `getpaths`, 79 reads of `fwriteptr` |
| `0x13e22e68` | `FILE *` of the text/binary file in use | 4 B | Shared by all readers | `loadtextfile` |
| `0xb7a8ca0` | `blocklist` | 257 KiB (`0x40360`) | List of block records (ELF symbol `blocklist`, 262,996 bytes) | New, `--min-votes 2` |
| `0x204020` | debug flag | 4 B | Never written | section 3.3 |
| `0x60d08` | packet magic `7DFP` | 4 B | First 4 bytes of every datagram | `docs/protocol.md` §3.1 |
| `0x60d18` | `__security_cookie` | 4 B | Stack cookie, used by 53 functions | section 6 |
| `0x25ef97d4` | SSE2 flag | 4 B | Set by `0x5d426`, read by `_ftol2` | section 6 |
| `0x2561d740` | world clock | 4 B | Advances by 10 per tick; drives the periodic save | section 3.1 |
| `0x16d66040` | `area` | 4.0 MiB (4 × `0x100d6c`) | Level areas. `createarea` allocates one volume block for each. Holds a heap pointer at `+0x48` of each record (`0x17166088`) | New. Record size matches the ELF symbol `area` (4,208,064 bytes) |
| about `0x9c1d8c0` | `landscape` height map | 16.0 MiB (2049 × 2049 floats + header) | Terrain heights written by `createlandscape` | New. Size matches the ELF symbol `landscape` (16,793,616 bytes) |
| `0x1954d700` | `area_sector` (start) | unknown | Sector records used by `area_createsector`, `area_createblock`. It starts right after `bsdsocket` | New. Size not checked (193 MiB in the ELF) |

Totals: the named globals of the first table (with `blocklist` below) add up to **111 MiB** of the 606.6 MiB. `area` and `landscape` add 20 MiB. About **475 MiB** have no name and no size yet; they are in the regions of section 4.2.

### 4.2 Regions without a name in the CSVs

Gaps between two named globals (RVA of the end of the first and the start of the next), largest first:

| Between | Gap | Remark |
| --- | ---: | --- |
| `bsdsocket` end `0x1954d700` → image end `0x25ef97e0` | 201.7 MiB | Starts with `area_sector`. The world clock (`0x2561d740`) and the CRT globals are also here. |
| `stats` `0x1b497a8` → `object` `0x5592600` | 58.3 MiB | Many references near `0x1c497c0`, `0x1d65a40`, `0x2f65bd0`, `0x536a06c`. |
| `blocklist` end `0xb7e9000` → `filelist` `0xe219200` | 42.2 MiB | |
| `itemtype` end `0x1035da00` → `timerlist` `0x12a81c60` | 39.1 MiB | |
| `parser` end `0x13e22e68` → `connection` `0x15e39020` | 32.1 MiB | |
| `numofbullets` `0x17529a40` → `bsdsocket` `0x1952d680` | 32.0 MiB | |
| `server` end `0x761eb00` → `option` `0x90dc340` | 26.7 MiB | |
| `cubevertex` end `0x601d4` → `timer` `0x14043a0` | 19.6 MiB | The initialised data and the CRT cookie are at the start. |
| `event` end `0x9bdd880` → `objecttype` `0xac51bc0` | 16.4 MiB | Contains the `landscape` height map (16.0 MiB). |
| `account` end `0xf8f7b68` → `intersect` `0x103484a0` | 10.3 MiB | |
| `objecttype` end `0xb0661c0` → `particle` `0xb3e7200` | 3.5 MiB | |

The objects of the Linux 0.24c server (symbol sizes, x64) that are 1 MiB or more and that have no address in the 024d yet add up to 508 MiB. They are the content of the gaps above, in some order: `area_sector` 193.1, `building` 54.1, `render_sector` 48.0, `area_interior` 42.3, `area_sectormodel` 38.0, `trackmodel` 32.0, `floorplan2` 18.0, `buildingtype` 16.1, `sprite_render` 10.7, `track` 5.5, `radix` 4.0, `model` 3.5, `render_array` 3.0, `tempmesh` 2.8, `modeltemp` 2.4, `sprite` 2.3, `cityobjecttype` 2.3, `level_editor` 1.8, `sprite_model` 1.6, `voice` 1.3, and six objects of about 1 MiB. Several of them (`render_*`, `sprite*`, `model*`, `skymesh`, `tiremark`) are render data that the server may never touch. The order in the gaps is not known: different subsets of these sizes fit the same gap, so **no guess is made here**.

To close this: extend the matching of `tools/re/map_globals.py` (its votes come only from functions whose name also exists in the reference binaries), then check each candidate with the access stride in the code (as done above for `area`).

## 5. Start-up order: from the entry point to the main loop

Verified with the disassembly (`0x5cd10`, `0x5c8d0`, `0x5d08c`) and the decompiled `loadsaveoptions`. For the packet part see `docs/protocol.md` §2.

### 5.1 Loader and C runtime (entry `0x5d316`)

1. The Windows loader maps the image, applies the 21,953 relocations if the base is not `0x400000`, zero-fills `.data` (`0x25e997e0` bytes) and binds the imports (`WSOCK32.dll` is the proxy DLL when the hooks are used).
2. `mainCRTStartup` (`0x5d316`) calls `__security_init_cookie` (`0x5d7f0`) and jumps to `__tmainCRTStartup`.
3. `__tmainCRTStartup` (`0x5d08c`): `__SEH_prolog4`; `HeapSetInformation`; takes the start-up lock (`InterlockedCompareExchange`, `Sleep(1000)` while another thread holds it; never happens here).
4. `_initterm_e` over `0x5e1b0`–`0x5e1c0` runs three initialisers:
   - `0x5d25c`: checks the PE header, `__set_app_type(1)` (console), sets `_fmode` and `_commode`, calls `_setdefaultprecision` (`0x5d7b5`, which is `_controlfp_s(0, _PC_53, _MCW_PC)`), `__setusermatherr`, `_configthreadlocale(-1)`.
   - `0x5d426`: `IsProcessorFeaturePresent(10)` into the SSE2 flag (RVA `0x25ef97d4`).
   - `0x5d478`: `SetUnhandledExceptionFilter(0x5d436)`.
5. `_initterm` over `0x5e1a4`–`0x5e1ac` runs one initialiser: `0x5d041` (`_atexit(0x5d570)`, then `__getmainargs`).
6. `__tmainCRTStartup` calls `main(argc, argv, envp)`. **`main` is `getpaths` (`0x5cd10`).** Its return value goes to `exit`.

### 5.2 `main` (`getpaths`, `0x5cd10`)

| # | Call | What it does |
| --- | --- | --- |
| 1 | `debug_resetlogfile` (`0x5280`) | Does nothing: the debug flag is never set. |
| 2 | `getcpufrequency` (`0xb4a0`) | `QueryPerformanceFrequency`; makes the counter-to-millisecond factor. |
| 3 | `setuptimers` (`0xb540`) | Names the profiler timers. |
| 4 | inline | Stores `fread` and `fwrite` in `freadptr` and `fwriteptr`. |
| 5 | `_getcwd` × 2 | Two path buffers of 512 bytes (RVA `0x916d2e0`, `0x916d4e0`). |
| 6 | `loadsaveoptions(0)` (`0x4e7a0`) | `_chdir` to the saved directory. Sets the defaults (port 27584, `servermaxbytespersecond` 100000, `maxnumofplayers` 16, `roundtime` 7, `gametype` 3, name `Agent <n>`, a random password, `Sub Rosa Server`). `srand(time)`. Reads `config.txt`. `_chdir` back. |
| 7 | `WSAStartup(0x101, &wsadata)` | WinSock 1.1. |
| 8 | `jmp dedicatedserverloop` | A tail jump. The loop never returns. |

### 5.3 `dedicatedserverloop` (`0x5c8d0`), before the loop

| # | Call | What it does |
| --- | --- | --- |
| 1 | `getmasterserverip` (`0x1ac40`) | TCP/HTTP to `www.crypticsea.com`. **Blocks** for up to about 63 s: a 3 s `select` for the connect, then two loops of 300 × `_sleep(100)` (send, then receive). Stores the master IP and port and 8 server names. Fallback `216.55.186.104:27590`. Prints `connection successful` or `connection failed`. |
| 2 | inline | Clamps `roundtime` to 1…30 and `maxnumofplayers` to 2…128. Copies the server name to `server+0x14` (no length limit). Sets `server+0x5c` (max players), `game+0x28` = 3600, `game+0x2c` = `roundtime` × 3600, `game+0x34` = `0x4ec`, `server+0`/`+8` = 1. |
| 3 | `disablesocket` (`0xd710`, really *open server socket*) | `socket(AF_INET, SOCK_DGRAM, 17)`, `bind(port)`, `ioctlsocket(FIONBIO)`. Then `printf("port:%d")`, `printf("socketenabled:%d")`. |
| 4 | `init_genrand(_time64(0))` (`0xad30`) | Seeds the Mersenne Twister. |
| 5 | `loadaccountsserver` (`0xe2e0`) | `user/server.srk`. Makes a random account key if the file is missing. |
| 6 | `leveleditor_load` (`0x8590`) | `data/city.sbc`. |
| 7 | `setupgame` (`0x502a0`) | Lists `data/texture/*.png`; then `setupbuildingtypes`; `area_createsblocks` (lists `data/block/*.sbl`, `area_createinteriorblocks` → `loadblock` for each file, Bézier tessellation, LODs); `setupbody`; `setupbullettypes`; `setupitemtypes`; `setupobjecttypes_cars`, `_helicopters`, `setupobjectypes_train`; `setupcityobjecttypes`. |
| 8 | inline | Sets four floats of an unnamed table (RVA `0x17179604`, stride `0x14`) to 100.0, clears `player[i].active` for the 256 slots, forces `gametype` to 3 unless it is 3 or 4 (`game+8`). |
| 9 | `resetgame` (`0x590f0`) | `saveaccountsserver`; `newspaper_load` (`readme.txt`); `createlevel` (`srand(0)`, `createlandscape`, `buildsomeshit`, `createarea`, streets, `building_load` for each building, `building_buildfloorplan`); then `srand(_time64(0))`; creates the first humans, items, vehicles and the scenario (`createhuman`, `createitem`, `createvehicle`, `createobject`, `scenario_createguards`, `scenario_createcar`, `economy_carmarket`, `economy_gunmarket`) and the first events. |
| 10 | `QueryPerformanceCounter` | Time base of the loop. |

`resetgame` runs again at each new round: it is also called from `logicsimulation`, `logicsimulation_round` and `logicsimulation_world`.

### 5.4 The main loop (about 62.5 ticks per second)

Each pass: `_sleep(1)`, read the counter, and continue only when 16 ms have passed since the last tick. If the server is late it runs up to 5 ticks in a row. The time base moves forward by 16 ms for each tick; from the 4th tick in a row it is set to the current time, so a long backlog is dropped. One tick does, in this order:

1. If `game+0x20` is 2: `playerai(i)` for every active player slot whose field `+0x1a38` is set (the AI players).
2. `servercycle` (`0x33570`): sends the master heartbeat (type `0x20`) every 1024 ticks and updates the byte budget of each connection (`servermaxbytespersecond` ÷ number of connections).
3. `serverrecv` (`0x33650`): reads and handles all datagrams.
4. `playersimulation` (`0xa2a0`).
5. If `game+0x20` is 0, 1 or 2: `particlesimulation`, `objectsimulation`, `humansimulation`, `itemsimulation`, `bondsimulation2`, `itemlogic`, `itemtimetolive`, `objecttimetolive`, `particletimetolive`, `bondtimetolive`.
6. `logicsimulation` (`0x5c580`): rounds, economy, scenario, periodic saves (calls `logicsimulation_world`, `logicsimulation_round`, `resetgame`).
7. `stoptimer`. Every 64 ticks, if the debug flag were set, one line is appended to `dedlog.txt`.
8. `serversend` (`0x4ed30`): builds and sends the packet to each client.
9. `ticks` (`0x90dc578`) is incremented by 1.
10. If `game+0x20` is 0, 1 or 2: `bulletsimulation`.
11. `eventsimulation` (`0x4061a0`).
12. If `game+0x20` is 0, 1 or 2: `bullettimetolive`.

The loop has **no exit**. `disablesocket` (the real one, `0xd840`) and `WSACleanup` are never called, and the server stops only when the process is killed. `saveaccountsserver` runs only at the places in section 3.1, so a kill can lose the changes since the last save.

## 6. The MSVC runtime code inside the `.exe`

The 024d links the start-up and security parts of the MSVC 2010 C runtime statically and imports the rest from `MSVCR100.dll`. The code that is inside the `.exe` is from RVA `0x5ce18` to the end of `.text` (`0x5d940`). Everything in this table is **not game logic**; a standalone server must supply an equal behaviour only where the last column says so.

| RVA | Name (`functions.csv`) | Size | What it does | Used by | In a standalone server |
| --- | --- | ---: | --- | --- | --- |
| `0x5ce18` | `__security_check_cookie` | 15 | `/GS` check: compares `ECX` with `__security_cookie` (RVA `0x60d18`). If different, jumps to `__report_gsfailure`. | 53 functions (47 live) | Nothing. The compiler's own stack protector (or none). `lift.py` already skips it (`SKIPPED_CALLS`, `docs/lift.md`). |
| `0x5ce50` | `__alloca_probe` | 43 | Stack probe for frames larger than one page (`___chkstk`). | `building_setupbuild` only | Nothing. The compiler adds probes. Or move the large local arrays to the heap. |
| `0x5ce90` | `_ftol2` | 145 | Converts the x87 value in ST0 to an integer by truncation. | 33 functions (25 live), see below | **Yes, as a C cast.** See below. |
| `0x5cf70` | `__aulldiv` | 104 | Unsigned 64-bit division. | `gettimeofday` (dead) | Nothing. |
| `0x5cd7c`–`0x5ce17`, `0x5d88c`–`0x5d93a` | 101 thunks (unlisted) | 6 each | `jmp [IAT]` for each import. | Calls to imports | Nothing. Direct calls to the new libraries. |
| `0x5d316` | `mainCRTStartup` | 10 | Calls `__security_init_cookie`, then `__tmainCRTStartup`. | Entry point | The C runtime of the new platform. |
| `0x5d08c` | `__tmainCRTStartup` | 345 | Heap option, start-up lock, runs the initialisers, calls `main`, then `exit`. | `mainCRTStartup` | The C runtime of the new platform. |
| `0x5d041`, `0x5d25c`, `0x5d426`, `0x5d478`, `0x5d570`, `0x5d7b5` | unlisted | small | Initialisers (`__getmainargs`, app type, `_fmode`, `_commode`, SSE2 flag, unhandled-exception filter, `_setdefaultprecision`) and the exit-time table runner. | `_initterm`, `_initterm_e`, `_atexit` | The C runtime, **plus the 53-bit x87 precision** if the port keeps x87 (section 7, R1). |
| `0x5d320` | `__report_gsfailure` | 262 | Crash report after a failed cookie check: `IsDebuggerPresent`, `SetUnhandledExceptionFilter`, `UnhandledExceptionFilter`, `TerminateProcess(GetCurrentProcess(), 0xC0000409)`. | `__security_check_cookie` | `abort()`. |
| `0x5d7f0` | `__security_init_cookie` | 155 | Seeds the cookie with time, process id, thread id, tick count and `QueryPerformanceCounter`. | `mainCRTStartup` | Nothing. |
| `0x5d436`, `0x5d492`, `0x5d52a`, `0x5d533` | `__CxxUnhandledExceptionFilter`, `__onexit`, `_unlockexit`, `_atexit` | 9 to 152 | The `atexit` table and the C++ exception filter. The game registers one exit function (`0x5d570`), which runs an empty table. | CRT only | Nothing. |
| `0x5d5b0`, `0x5d5f0`, `0x5d640` | `__ValidateImageBase`, `__FindPESection`, `__IsNonwritableInCurrentImage` | 53 to 166 | Walk the PE header to check that an exception handler is inside the image. | `__tmainCRTStartup`, SEH | Nothing. PE specific. |
| `0x5d710`, `0x5d755`, `0x5d769` | `__SEH_prolog4`, `__SEH_epilog4`, `__except_handler4` | 20 to 69 | Structured exception handling frames with a cookie. | `__tmainCRTStartup`, `__onexit` only. **No game function uses SEH.** | Nothing. |

Data that belongs to the runtime: `__security_cookie` (`0x60d18`), the SSE2 flag (`0x25ef97d4`), the start-up state and lock (`0x25ef97c4`, `0x25ef97c8`), the encoded pointers (`0x25ef97cc`, `0x25ef97d0`) and `argc`/`argv`/`envp` (`0x204024`–`0x20402c`).

### `_ftol2` in detail

`_ftol2` is the only runtime function that game code depends on for its *numbers*. Its two paths:

- **SSE2 path** (taken on any CPU with SSE2 when the x87 control word has all exception masks set, which is the case here): `fstp qword [esp]` then `cvttsd2si eax, [esp]`. The value is **first rounded to a `double`** (53 bits), then truncated to a 32-bit integer. Only `EAX` is valid.
- **x87 path** (old CPUs): a 64-bit `fistp` with a fix-up that gives truncation toward zero, result in `EDX:EAX`.

A standalone server should use the C cast `(int)(double)x`. It differs from a plain truncation of the 64-bit mantissa only for values that are within 2^-53 (relative) of an integer. `lift.py` writes the 64-bit form `(longlong)` (`INLINE_CALLS`), because the Ghidra prototype of `_ftol2` returns `EDX:EAX`. On the SSE2 path `EDX` is not set, so a caller that read it would already be wrong on a current CPU. The live callers include money code (`playerbuystock`, `playersellstock`, `createevent_updatestocks`, `logic_addteamcash`, `playerdeathtax`, `playerteamkilltax`, `logicsimulation_world`), the ray and sphere tests on the landscape and areas, `createtrack`, `playersimulation`, `bulletsimulation`, `humansimulation`, `objectsimulation`, `objectsimulation_car`, and `packobjectpacket`.

### What the standalone server needs instead (checklist)

| Need | In the 024d | Replacement |
| --- | --- | --- |
| A `main` | `getpaths` (`0x5cd10`) | Same steps (section 5.2), without the CRT tables. |
| Heap, `memcpy`, `memset` | `MSVCR100` | libc. |
| `rand` / `srand` | MSVC LCG, 15 bits | Any generator that returns 0…32767 (R2). The MSVC one is optional, for replayable tests. |
| `_CI*`, `floor` | `MSVCR100` x87 | `libm`, or inline x87 on x86 (R1). |
| `_ftol2` | Static CRT | C casts (above). |
| Clock | `QueryPerformanceCounter`, `_sleep(1)`, `_time64`, `RDTSC` | `clock_gettime(CLOCK_MONOTONIC)`, `nanosleep`, `time`. Drop the profiler or use the monotonic clock. |
| Sockets | `WSOCK32` ordinals | BSD sockets. `WSAStartup` has no equivalent. `ioctlsocket(FIONBIO)` becomes `fcntl(O_NONBLOCK)`. |
| Directory scan | `_findfirst64i32` | `opendir`/`readdir` with the glob match; the result is sorted by `qsort`, so the order does not depend on the file system. |
| Working directory | `_chdir`, `_getcwd` | `chdir`, `getcwd`. |
| Files | `fopen` (text and binary modes) | `fopen`. Text mode for `stats.txt` writes CR CR LF on Windows. |

## 7. Risks for Phase 4

| ID | Risk | Evidence | First step |
| --- | --- | --- | --- |
| **R1** | **x87 precision and rounding.** About 40 % of the instructions are x87 and the program runs with 53-bit precision. A build with SSE2 (every x64 build, and `/arch:SSE2`) computes `float` expressions in 24 bits and rounds at other places, so results differ in the last bits. | `_controlfp_s(0, 0x10000, 0x30000)` at `0x5d7b5`. 41,955 x87 instructions in 271 functions. Known: `createlandscape` differs in 263 bytes (over 4 ulps) of the 2049 × 2049 height map and `buildsomeshit` in about 6000 bytes (`docs/lift.md`). 160 `fldcw`/`fnstcw` in the packers (`packobjectpacket`: 63, `serversend`: 6, ...) set round-toward-zero around `fistp`, so the **bits sent to the clients** depend on this rounding. `_CIpow` is used by the packers. `_CIsin`, `_CIcos`, `_CIatan2`, `_CIacos`, `_CItan` use x87 instructions in `MSVCR100`; `libm` can differ by one ulp. | Decide the goal: bit-exact (keep x87 on 32-bit Windows, `/arch:IA32`, and use inline `fsin`/`fpatan` for the rest) or "same within N ulps" with a test for each packer. Evaluate float expressions in `double` and round to `float` only where the original stores (`FLT_EVAL_METHOD` 1 style). |
| **R2** | **Random numbers (low).** `rand()` is the MSVC generator (15 bits). It is used for AI, the economy, scenarios, damage, spawn choices and the look of new accounts, and it is seeded from the clock (`srand(_time64(0))` in `loadsaveoptions` and `resetgame`), so production runs are not repeatable anyway. | 21 functions, 47 sites. Every use is `& mask` or `% n` (`idiv`) with a small `n`; none divides by `RAND_MAX`. `createlevel` calls `srand(0)` (`0x54e55`) but none of the 54 functions below it calls `rand`: the level does not depend on it. The Mersenne Twister (`init_genrand`, `genrand_int32`) is used only for the account key (`loadaccountsserver`, `createaccount`); in the x64 ELF its state is an `unsigned long` array, so use `uint32_t`. | Provide `sr_rand()` with the 0…32767 range. Copy the MSVC linear congruential generator only if replayable tests are wanted. |
| **R3** | **Pointers inside structs, and 64 bits.** The structs that go on the wire or into files have the same size on x64, but level and runtime structs hold pointers as 32-bit ints. | `area_block` record: `0x84` bytes in the 024d, `0xb0` in the x64 ELF, with 4 `malloc` pointers at `+0x44`…`+0x50`. `area` record: pointer at `+0x48`. Other symbols that grow in x64: `render_sector` (+16 MiB), `area_block` (+2.75 MiB), `model`, `timer` (22,528 against 21,504 bytes), `mt` (4,992 against 2,496), `building_editor`, `area_interior` (ELF symbol size against the gap between Mach-O symbols, so the i386 values are approximate). `freadptr`, `fwriteptr` and the `FILE *` at `0x13e22e68` are pointers in globals. Sockets are stored as 32-bit ints at `bsdsocket+0x24`. Same size on both: `player`, `human`, `object`, `item`, `connection`, `account` (`docs/structs.md`). | Give every pointer field its own type (`void *`) in `sr_types.h` and add `static_assert`s for both widths. Keep the on-wire and on-disk layouts by `#pragma pack`/fixed-width types. |
| **R4** | **Pool limits are inside the code.** Loops end at absolute addresses (`cmp eax, 0xf42e500` is `&player[256]`), masks are literal (`& 0x3fff`), and some limits are part of the protocol or the file format. | 21,925 absolute addresses in `.text`. `event` is 65536 because the client acknowledges with 16 bits. `account` is 65536 because the file count is `0xffff` at most. `player` is 256 but `maxnumofplayers` is clamped to 2…128. | Make each limit a named constant first (`SR_MAX_*` exist), keep the protocol-bound ones fixed, and only then switch the others to heap arrays. Check `docs/protocol.md` for the bit width of each index before raising a limit. |
| **R5** | **Readers trust sizes from the file.** | `loadtextfile` copies the whole file into the 1 MiB `parser` buffer and the next global is the `FILE *`. `loadaccountsserver` reads the count and does not compare it with 65536. `leveleditor_load`, `building_load` and `loadblock` read counts the same way. `dedicatedserverloop` copies the server name with no limit. | Keep the behaviour for valid files first. Add the checks in Phase 5 (with the input checks of `docs/plano.md`, item 5.6) and fuzz the readers with `tools/formats`. |
| **R6** | **Blocking network call at start-up.** `getmasterserverip` waits up to about 63 s (a 3 s `select` for the connect, then 30 s to send and 30 s to receive, in steps of 100 ms) and uses `gethostbyname`. The master server (`www.crypticsea.com`) may not answer any more. | `0x1ac40`: `select` timeout 3 s, then `_sleep(100)` loops of 300 passes. Fallback `216.55.186.104:27590`. | Run it with a time limit or in a thread, and make the master list optional (`docs/plano.md`, item 5.4). |
| **R7** | **Loss of the test oracle.** The Phase 3 verifier runs the original function next to the rewrite inside the `.exe`. A standalone server has no original. The lifted C code is also tied to the image: it uses `SR_VA()` addresses, `sr_import` and naked thunks that save the x87 state. | `docs/lift.md`. `src/lifted/lifted_024d.c`, `hook/hooks_lifted.c`, `hook/imports.c`. | Before the `.exe` goes away, record golden traces: the harness inputs (`sv_args.txt`, `sv_snapshot.bin`) and a packet capture of a played session. Replace `SR_VA(...)` by real variables function by function. |
| **R8** | **Coverage of `functions.csv` and wrong names.** 7 live functions are not in the CSV (6 CRT and the `qsort` comparator `0xd320`). 129 unlisted code blocks (found by the padding rule) and 32 listed functions have no reference. Three names mislead: `getpaths` is `main`, `disablesocket` (`0xd710`) opens the socket, `optionreadwriteint` (`0x29650`) is a key search. | Section 8. | Fix the CSV (`manual_names.csv`) before the Phase 4 port starts, and decide which dead functions to port. The client-only ones (`clientrecv`, `unpack*`, `saveaccountsclient`) are needed again in Phase 6. |
| **R9** | **Time and profiler.** The tick is 16 ms with up to 5 catch-up ticks. `_sleep(1)` has a coarse step on Windows. The profiler uses `RDTSC` 24 times. | `0x5c8d0`, `0xb4a0`. | Use a monotonic clock and a precise sleep. Drop the profiler or move it to `clock_gettime`. No game rule depends on the TSC. |

The facts that make Phase 4 easier: the server has **one thread**, uses only **UDP plus one TCP request**, opens **a small set of files**, allocates the **heap in two places**, and every platform call is in the tables of section 2.

## 8. Notes found while making this inventory

### 8.1 Names that do not match the code

These are for the next review of `analysis/024d/manual_names.csv`. This document did not change any CSV.

| RVA | Name in `functions.csv` | What the code does | Evidence |
| --- | --- | --- | --- |
| `0x5cd10` | `getpaths` | It is the C `main`. `__tmainCRTStartup` calls it with `argc`, `argv`, `envp`. It saves the directory, loads the options, calls `WSAStartup` and jumps into `dedicatedserverloop`. | Section 5.2. |
| `0xd710` | `disablesocket` | Opens the server UDP socket: `socket`, `bind(port)`, `ioctlsocket(FIONBIO)`. The ELF calls this `enablesocketserver` (213 bytes). `0xd7b0` (unlisted) is `enablesocketclient` and `0xd840` (unlisted) is the real `disablesocket`. | Disassembly; `analysis/symbols/subrosa_x64.functions.txt`. |
| `0x29650` | `optionreadwriteint` | A key search: advances the `parser` cursor to the text of `key=`. `loadsaveoptions` calls it before every value. | Decompiled `loadsaveoptions`. |
| `0x296a0` (unlisted) | none | Stores the addresses of `fread` and `fwrite` in `freadptr` and `fwriteptr`. `main` has the same code inline. No caller. | Disassembly. |
| `0x1000` (unlisted) | none | `time()` wrapper (`push eax; call _time64`). No caller. | Disassembly. |

`docs/protocol.md` §2.3 gives `0x12c90` as the 024d RVA of `servercycle`. `0x12c90` is `bondtimetolive`; `servercycle` is `0x33570` (the heartbeat code is there).

### 8.2 Live functions that are not in `functions.csv`

| RVA | What it is |
| --- | --- |
| `0xd320` | The string compare used as the `qsort` comparator in `listfiles` (compares two bytes per loop). |
| `0x5d041` | `_initterm` initialiser: `_atexit(0x5d570)`, `__getmainargs`. |
| `0x5d25c` | `_initterm_e` initialiser: app type, `_fmode`, `_commode`, `_setdefaultprecision`, `_configthreadlocale`. |
| `0x5d426` | `_initterm_e` initialiser: SSE2 flag. |
| `0x5d478` | `_initterm_e` initialiser: `SetUnhandledExceptionFilter`. |
| `0x5d570` | Exit-time table runner. |
| `0x5d7b5` | `_setdefaultprecision`: `_controlfp_s(0, 0x10000, 0x30000)`. |

### 8.3 Listed functions with no reference in the program (32)

They stay in the CSV. The standalone server does not need them; the client-side ones may be needed again in Phase 6.

- Client and editor: `clientrecv`, `saveaccountsclient`, `loadaccountsclient`, `building_save`, `leveleditor_save`, `cs_pread`, `pbitreadnego`, `unpackevent`, `unpackobjectpacket`, `unpackorientation`, `unpackangle`, `unpackpositiondelta`.
- Packers that the server loop does not use: `packobjectpacket2` (a second variant of `packobjectpacket`), `packpositiondelta`, `packscalar`, `packunitvector`.
- Maths and simulation: `lerpnoise2d`, `cerpnoise2d`, `rotatevector2d`, `lineintersectline2d`, `sphereintersectline2d`, `lineintersectcircle2d`, `sphereintersectpoint`, `logicsimulation_driving`, `object_rotorsimulation`, `economysimulation`.
- Time, CPU and random: `systemtimer`, `testtimer`, `gettimeofday`, `setcpuaffinity`, `init_by_array`, `__aulldiv`.

In addition, 129 code blocks that the padding rule marks as separate functions and that `functions.csv` does not list have no reference. Examples: `0x6a50` (local IP list), `0xd7b0`, `0xd840`, `0xd4e0`/`0xd5a0` (text helpers with `fgetc`/`fputc`), `0xde90` (`_getcwd`), `0x15ca0`, `0xbf30`, `0xcf90` (maths).

### 8.4 Next steps suggested for the Phase 4 start

1. Finish the map of `.data` (section 4.2): extend the matching of `map_globals.py` and check each array with its access stride.
2. Fix the names of section 8.1 and add the functions of section 8.2.
3. Choose the numeric goal for R1 and write one test for each packer.
4. Record the golden traces for R7 while the original `.exe` still runs.
5. Write the platform layer from sections 2 and 6 (`src/platform/`: sockets, timer, files, directory scan) and run the first standalone start-up up to the end of `setupgame`.

### 8.5 How to repeat the main numbers

```bash
bash tools/cloud/setup.sh && bash tools/ghidra/analyze_all.sh
sha256sum original/blackhouser/subrosadedicated024d.exe  # must be 11e00678…, as in original/MANIFEST.sha256
objdump -d -M intel -j .text original/blackhouser/subrosadedicated024d.exe > text.dis
# imports: parse the PE import directory (RVA 0x5e000 area); IAT slots are 0x5e000-0x5e19c
# calls to an import: "call DWORD PTR ds:0x45e0xx" and "call 0x45cxxx" / "call 0x45d8xx" (thunks)
python -I tools/re/map_globals.py --target build/features/ded024d.jsonl --names analysis/024d/functions.csv \
    --ref build/features/mac_i386.jsonl --ref build/features/linux_x64.jsonl --ref build/features/ded024c_linux.jsonl \
    --min-votes 2 --out /tmp/globals_try.csv           # adds option, area_block, blocklist
tools/ghidra/decompile.sh subrosadedicated024d.exe /tmp/dec.c 0x1ac40 0x4e7a0   # as in lift.md, program name in the project
```

`python -I tools/re/manifest.py check` prints 385 `MISMATCH` lines in a cloud session. The manifest lists the whole `original/` folder of the PC (archives, configs, saves, old packages), and `check` counts a file that is missing here as a mismatch. The five binaries used in this document were compared with `original/MANIFEST.sha256` one by one and are equal: `blackhouser/subrosadedicated024d.exe`, `client/subrosa.x64`, `client/subrosa.exe`, `client/subrosadedicated.exe` and `client/subrosa.app/Contents/MacOS/subrosa`.
