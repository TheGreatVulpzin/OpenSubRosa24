# Rewrite of the client (step 7.6)

How a group of the client rewrite works. The plan and the group list are in [`docs/plano.md`](plano.md), "The groups of the client rewrite". The state of the client is in [`docs/client.md`](client.md).

## What a rewrite is

The client table (`src/lifted/client/lifted_table.c`) holds one C function for each game function. Today all of them are lifted C (`src/lifted/client/lifted_client.c`, frozen). A rewrite moves one function into clean C in `src/client/` (code that only the client has) or in `src/common`, `src/net`, `src/game` (code that the server also has). The table then points to the clean function. The result must be the same as the original client for every call that the gate or a synthetic test makes.

## Files

| File | Use |
| --- | --- |
| `analysis/client_win/groups.csv` | Every function of the table with its group (C0 to C11), size and number of recorded calls. `python -I tools/re/client_groups.py` writes it. |
| `analysis/client_win/adopted_C<n>.txt` | The functions of group C<n> that use clean code. One line each: `name path`. `lift.py` reads every `adopted*.txt`. The file `adopted.txt` is group C0. |
| `analysis/client_win/adopted_C<n>_pending.txt` | Functions that are written but that no test reaches yet. They stay lifted. |
| `analysis/client_win/group_globals_C<n>.csv` | Globals that only the client has: `name,type,count,client_rva,evidence`. The clean code reads them as `cl_data_<name>`. |
| `analysis/structs/names.csv`, `tools/re/gen_types.py` | Names of fields and types. Run `gen_types.py` after a change. |
| `analysis/client_win/manual_names.csv` | Names of functions (`rva,name,evidence`). |
| `src/client/client.h` | `CL_FN(va, type)` calls a function that is still lifted. |
| `src/client/<system>/*.c` | Clean code that only the client has, in one folder for each system (see "Folders"). One file for each topic (`image/png.c`, `image/jpeg.c`, `image/fonts.c`). |

## Steps of a group

1. **Plan the group.** List the functions: `awk -F, '$4=="C1"' analysis/client_win/groups.csv`. Read the lifted C of each one (marker `/* --- 0x<rva> */` in `src/lifted/client/lifted_client.c`). The Linux client has names and shapes: `original/client/subrosa.x64` (use `tools/re/elf_symbols.py`). Decide the files and the order. Small functions that others call come first.
2. **Names.** Name the functions you write (`manual_names.csv`), the globals (`group_globals_C<n>.csv`) and the struct fields (`names.csv`). A global that the server also has is `sr_data_<name>` (see `analysis/client_win/storage.csv`). Do not run a new Ghidra export for names.
3. **Write the clean C** in `src/client/` with the same function name as the table. Keep the same arguments, the same result and the same writes to memory. Call a function that is not rewritten yet with `CL_FN`. Comments say what the code does, in short STE sentences. Never write about the original, the lifted C or the process.
4. **Adopt.** Add `name path` to `adopted_C<n>.txt`. Run `bash instrumentation/client/pipeline.sh --no-gate`. Search `build/pipeline_client/build.log` for `error C` and `error LNK`.
5. **Test.** Run the gate with every recorded call of the group: `python -I instrumentation/lift/gate.py --target client_win --jobs 2 --all-calls --funcs name1,name2`. Results are in `build/gate/client_win/reset/all/`. A function with no recorded call needs a synthetic test (see below). `python -I instrumentation/client/adopt_from_gate.py --file analysis/client_win/adopted_C<n>.txt` sorts the functions into tested, untested and failed. With `--keep-tested` it keeps the tested ones and moves the others to the pending file.
6. **Check the whole client.** Before a commit that others will use, run `python -I instrumentation/lift/gate.py --target client_win --jobs 2 --changed`. The counts of "same" must not drop (menu 85, play 285, play2 280 on 2026-11-09).
7. **Document.** Add the group to `docs/client.md` (what the system does, the files, the open points). Mark the group in `docs/plano.md`.

## Tests for functions that no recording reaches

`instrumentation/client/synth_calls.py`, `synth_humans.py` and `synth_voice.py` write call files for the harness. For a new group, add a script `synth_<group>.py` in the same style. A good test calls the original and the clean function with the same input and compares the result and the memory. Examples: decode the same image file with both versions, call a texture generator with several seeds, call an input function with a made keyboard state. State the cases and the result in `docs/client.md`.

## Folders

`src/game` has 60 files in one folder. Do not copy that. The client code goes into one folder for each system, with the group in the name of the folder:

| Folder | Group | Content |
| --- | --- | --- |
| `src/client/image/` | C1 | PNG, zlib, JPEG, BMP, PSD, HDR, fonts |
| `src/client/textures/` | C2 | Generated textures, models, landscape, noise |
| `src/client/render/` | C3 | Frame, sprites, shaders, HUD drawing |
| `src/client/input/` | C4 | Keyboard, mouse, controls |
| `src/client/menus/` | C5 | Menus and their items |
| `src/client/audio/` | C6 | Sound and voice |
| `src/client/setup/` | C7 | Start, loop, timing, debug |
| `src/client/editors/` | C8 | Level, building and model editors |
| `src/client/gl/` | C9 | OpenGL loader |

Groups C10 and C11 put each function in the folder of its topic, and make a new folder when none fits. `src/client/client.h` stays at the top. A file includes another with its folder: `#include "image/png.h"`. Each folder has a `README.md` of two or three lines that says what it holds.

## Shape of the code

The clean client code must be as tidy as the clean server code. Models: `src/game/items.c`, `src/game/items.h`, `docs/game/items.md`.

- One `.c` and one `.h` for each topic, in the folder of its system. The header has a one line comment, the include guard and only the functions that other files use, grouped by source file. What only the `.c` uses is `static`.
- The `.c` starts with a comment of 3 to 8 lines about the system (what it does, the format, who calls it). Each function has a short comment: what it does and when it fails.
- No `unk_`, `local_`, `iVar`, `param_` names and no loose magic numbers. Structs have named fields (`analysis/structs/names.csv`, `tools/re/gen_types.py`). Constants have names.
- A function keeps the name it has in the Linux client (`original/client/subrosa.x64`). New helpers get short, clear English names.
- A big function may split into small `static` helpers. The function of the table keeps its name, arguments and result.
- No dead code, no commented code, no repeated blocks. Exception: keep a line that looks dead when it changes memory that the game reads later or decides a result. Say in plain words what it does. The technical reason goes to the page in `docs/client/`.
- Comments and the pages of `docs/client/` describe the code only. They never name the original, the lifted C, the rewrite, the gate, a phase, a step or a group.
- One doc for each system in `docs/client/<system>.md` (the folder has a `README.md` with the list), in the style of `docs/game/*.md`.

## Rules

- A clean function is adopted only when a test shows the same result as the original client. No test means no adoption.
- The clean function is not slower than the original. Measure with `python -I instrumentation/lift/bench.py --funcs <names>` for functions that run every frame.
- Two failed tries to fix a float difference in the last bits: stop, write the function, the bytes and the checks you did into your report.
- Never edit `src/lifted/client/lifted_client.c` or `lifted_table.c` by hand. Never add a text rule to `lift.py`.
- Do not commit the two generated files in a group branch. Run `git checkout -- src/lifted/client` before each commit. The integrator runs the pipeline after the merge and commits them once.
- Comments: ASD-STE100, no emojis, no dashes as punctuation. Before each commit run `grep -n -i "original\|lifted\|rewritten\|024d\|harness" <files you wrote>`. It must find nothing.
- Commit messages in English. No AI co-author line. No push, no tag.
- Never run the gate, the bench or a build while the user plays the game.

## Parallel work

One group at a time works in the main folder, on its own branch (`git switch -c client/c1`). The build folder (`build/`) holds files that are not in git: the recordings, the Ghidra export (`build/lift/client_win.jsonl`), the game folder for the tests (`build/client_test`, `build/client_deps`) and the compiled tools. Two sessions that build in the same folder break each other.

Two groups at the same time need two full copies of the project folder, each with its own `build/` (copy it, do not link it). The copy is large (several GB with the recordings). Use it only when the time saved is worth that.

Two groups must not write the same file. `docs/client.md` and `docs/plano.md` take one section for each group, so a merge conflict is small. `analysis/client_win/functions.csv` and `manual_names.csv` take only added lines. After a group ends, the integrator merges the branch into `main`, runs `bash instrumentation/client/pipeline.sh --no-gate` and commits the two generated files in `src/lifted/client/`.

## Commands

| Command | Use |
| --- | --- |
| `python -I tools/re/client_groups.py` | List the groups. |
| `bash instrumentation/client/pipeline.sh --no-gate` | Lift, tables, build (3 to 4 minutes). |
| `python -I instrumentation/lift/gate.py --target client_win --jobs 2 --all-calls --funcs a,b` | Test some functions with every recorded call. |
| `python -I instrumentation/client/adopt_from_gate.py --file analysis/client_win/adopted_C1.txt` | Sort the functions of a group by the gate result. |
| `python -I tools/re/check_adopted.py --file analysis/client_win/adopted_C1.txt` | Find functions that reach data with no client address. |
| `python -I instrumentation/client/check_variadic.py` | Find printf-like calls with too few arguments. |
