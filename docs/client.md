# Client decomp (Phase 7)

Target: `original/client/subrosa.exe`. It is the 0.24b client of 2015-08-02, built with MSVC 2010. Ghidra name of the program: `client_win`.

## Name sources

| Source | Use |
| --- | --- |
| `original/client/subrosa.x64` | ELF with 2757 function symbols. Main source of names. |
| 0.24b Mach-O i386 (`mac_i386`) | Same version as the exe. Second source. |
| 0.25b Mach-O i386 (`mac025_i386`) | Not downloaded. The 0.24b Mach-O is closer to this exe, so it adds little. |
| Machine code | GLee names, see below. |

`tools/re/name_all.sh` runs the matcher and the GLee step. The output is `analysis/client_win/functions.csv`.

## Files in `analysis/client_win/`

| File | Content |
| --- | --- |
| `functions.csv` | All 2907 functions. Columns: `rva,size,name,source,confidence,evidence`. Generated. |
| `glee_names.csv` | Names from `tools/re/name_glee.py`. Generated. Merged as manual names. |
| `globals.csv` | 47 global candidates from `map_globals.py`. Generated. See Globals below. |
| `globals_checked.csv` | Status of each candidate, from `tools/re/check_globals.py`. Generated. |
| `manual_globals.csv` | The 1595 GLee function pointers, named `GLeeFuncPtr_<function>`. Generated. |
| `manual_names.csv`, `data_globals.csv`, `params.csv`, `locals.csv` | Same format as the files in `analysis/024d/`. Empty for now. |

## GLee

The client links GLee statically. About 1750 of the 2907 functions belong to it. They have two fixed shapes:

- **Link function** (`__GLeeLink_<extension>`). It calls `GetProcAddress("glXxx")` and stores the result in a pointer global. `tools/re/name_glee.py` reads the string and the global from the machine code. It gives the function the name of the Linux function that loads the same strings.
- **Stub** (`GLee_Lazy_glXxx`). It calls a check function for the extension, then calls through the pointer global. The pointer name gives the stub name.

Not named: 3 link functions with no Linux match (`0x1e370`, `0x1e910`, `0x20fd0`) and the wgl link functions, which exist only on Windows.

## Globals

`tools/re/map_globals.py` gives 47 candidates in `globals.csv`. They are big arrays such as `human`, `player`, `building`, `city` and `texture`. The ELF and the Mach-O have only function symbols in the features, so each vote comes from a named function that uses the global.

`tools/re/check_globals.py` checks each candidate against the code of the exe and writes `globals_checked.csv`. Two kinds of evidence:

- **Layout:** the candidate ends exactly where the next one starts. The sizes come from the reference binary, so an exact chain is strong.
- **Access:** the code has many 32-bit addresses inside the range of the candidate.

| Status | Count | Meaning |
| --- | --- | --- |
| confirmed | 6 | Exact layout chain: `area_sectormodel`, `timerlist`, `speex`, `area_block` (start only, its size runs into `parser`), `filelist`, `city`. |
| likely | 24 | Strong votes, the size fits, many accesses. |
| unverified | 6 | Strong votes, the size fits, few accesses: `cs_steam`, `programobject`, `keyboard`, `buildblocktype`, `input`, `masterserver`. |
| doubtful | 11 | Weak votes, or the size runs into the next candidate: `bmask`, `socketid`, `lightwavelength`, `stats`, `clientaccount`, `buttonmesh`, `model_human`, `sphere`, `texture_windowatlas`, `keyboardlabel`, `area_sector`. |

The check is not a proof. A struct field access can look the same for two arrays. Only Ghidra on the real code can close the unverified and doubtful ones.

No candidate is in `manual_globals.csv` yet. That file needs the element count, and the candidates give only the byte size.

`tools/re/map_regions.py` works only for the 024d (fixed paths). A client version needs new paths first.

A cycle of matcher and globals, as for the 024d, gave 9 new names and 6 renames. Three of the six renames lowered the confidence. The cycle is not in `name_all.sh`.

## Open work

About 630 functions of the game code have no name. They sit in `0x0` to `0x10000` and from `0x30000` up. The server functions (`analysis/024d/functions.csv`) cannot name them without circular evidence, because the 0802 server names come from this client.

See `docs/plano.md`, Phase 7, for the steps.
