# Standalone smoke test

Run this test on Windows with a display and the real OpenGL driver. Run one client at a time.
The driver sends SDL events. Polling wrappers supply keyboard, mouse button and relative motion states.

```powershell
python -I instrumentation/client/smoke.py build/client_test/opensubrosa24_client_smoke2.exe
python -I instrumentation/client/test_smoke.py
```

Build the client with `tools/build_client.bat`. Copy its output to a new executable name in `build/client_test`.
Do not replace an executable used as a comparison reference.
After an edit to either `.inc` file, force the compiler to rebuild `client_smoke.c`.
Check that its object file is newer than both includes. Incremental builds can miss these includes on this machine.
The executable must contain the current smoke driver. Older executables cannot produce captures.

Select scenarios after the executable path: `menus play crash buildings level model`.
The default runs all six scenarios. Use `--timeout <seconds>` to set a process limit.

- `menus` opens Options, Voice Options, Start and Join. It captures each main screen.
- `play` opens Start, disables network hosting and starts a local round.
  It selects a team and Ready, then waits up to 180 seconds for the player.
  It walks for two seconds, turns with relative mouse input and jumps.
  Insert supplies debug money. The shop supplies a 9mm gun and its magazine when needed.
  The driver selects the gun, loads the magazine with Shift and R, then waits for the active round.
  Four left clicks test pixel changes, sound count increases and ammunition or gun state changes.
  Each missing result fails with a separate message. Sound counts do not prove audible speaker output.
- `crash` captures the building list, then opens the first building and captures the game view.
- `buildings` captures the list, then opens each building and captures its game view and editor.
- `level` captures the demo and level editor. `model` captures the debug model viewer.

The driver reads `GL_BACK` with `glReadPixels` immediately before `SDL_GL_SwapWindow`.
It writes uncompressed 24 bit BMP files without an image library.
Each capture records the black pixel fraction, distinct color count and mean RGB value.
An all-black image or a single-color image fails. The executor checks the saved pixels again.
Open the images to check text, geometry and visual defects that pixel counts cannot identify.

Results use `build/client_smoke/<exe>/<scenario>/`:

- `shots/NN-name.bmp`: framebuffer images.
- `shots.jsonl`: capture metrics. Each complete line survives a client crash.
- `screens.json`: visible menu controls and field values.
- `run.log`: stages, checks and errors.
- `index.md`: image links, metrics and check results.

Each executable also has `summary.json`. The entire output folder is ignored by Git.
The executor restores settings, account files, city data and building files after each process.

Use `--shots main,standing,before-fire,after-fire` to select capture points by name.
The default, `--shots '*'`, selects all points. `play` requires both fire captures.
Use `--output <folder>` to change the root folder.

```powershell
python -I instrumentation/client/smoke.py build/client_test/opensubrosa24_client_smoke2.exe menus --compare build/client_test/other.exe
```

Comparison runs the same scenarios in sequence, once per executable.
`comparison.json` records the percentage of different pixels for each matching capture.
Size differences count as 100 percent. Missing captures and image differences produce warnings.
These warnings do not change scenario results. Process failures and invalid captures fail as usual.
The play scenario sets voice chat and push to talk, with V as the key and no mouse binding.
It supplies deterministic microphone samples through the capture imports, without a physical microphone.
It records the voice ring state before and during fire. A fire click that starts voice transmission fails the scenario.
A V press must start transmission. Transmission must stop after the release interval.
The executor restores the settings file after the process ends.

The play captures include `round-before-team`, `round-after-team`, `shop` and `weapon-shop`.
The round screen is a roster with three team columns and a fourth column for observers.
An empty team has no player rows. Its gray panel uses an untextured rectangle.
The driver checks team selection, then opens the shop after the player enters the round.
The play fixture omits the gain key. After exit, the saved file must contain the default gain and all talk settings.
The saved file must use CRLF lines without an extra carriage return.

Use SR_SMOKE_ROUND_ONLY=1 for roster comparisons. This mode ends play after the team captures and does not test fire or voice.

## Native executable and guards

Run the native executable with one command:

```powershell
bash tools/native_test.sh --guard --hidden menus level
```

The script builds the targets, checks the build and runs the scenarios. It stops with exit code 1 when:

- the build prints an error or returns a code other than 0 (`tools/build_native.bat` does the build);
- an executable is older than a source file, so a failed build cannot leave an old executable to test;
- a scenario fails, or a guard fires (see below).

`--hidden` hides the game window. The run takes no screen, no focus and no mouse: the driver supplies the mouse
motion and the keys, and the call that holds the cursor (`SDL_SetRelativeMouseMode`) does nothing in the smoke build.
`--build-only` builds and checks the executables only. Without `--guard` the script tests the normal executable.

The native executable keeps each datum as its own C object. A write past the end of an object changes the
next variable in memory, and the next variable is not the same one as in the first executable.
The guard targets add 256 bytes after each object (`SR_STORAGE_GUARDS`). A thread checks all guards every
millisecond. When a guard changes, the process log has a `GUARD <name> first=<n> last=<n>` line and a
`SAMPLE` line with the address of the main thread and its return addresses. The script maps them to function
names (`tools/re/map_address.py`). The scan repairs the guard, so the next write is reported too.
A guard that fires means a size in `analysis/client_win/group_globals_*.csv` is too small, or an array that the
original executable fills past its end into idle data. Give the real size, or add the object to
`analysis/client_win/native_slack.csv`.

### Tools of the native test

- `tools/build_native.bat <target> ...` builds native targets. Its exit code is 0 only when the build passes.
  The log is `build/native_build.log`. Targets: `subvulpes-client-native`, `-smoke`, `-guard`, `-guard-smoke`.
- `tools/native_test.sh [--guard] [--hidden] [--build-only] [--timeout N] [scenario ...]` builds, checks that the
  executable is current and runs the scenarios (default: `menus level`). Output goes to `build/nt/<plain|guard>`.
  Keep the path short: the capture path must stay under 90 characters.
- `tools/re/gen_client_native.py` makes `src/client/storage.c` and `src/include/sr_client_storage.h` from
  `analysis/client_win/*.csv`. It also makes the guard holders and `cl_guards_scan`.
- `analysis/client_win/native_slack.csv` gives extra bytes to an object that the original code fills past its end.
- `tools/re/map_address.py <exe>.map <file>` turns the `SAMPLE` and `CRASH` addresses of a log into function names.
  Put each `SAMPLE` line on its own line first.
- `SR_SMOKE_HIDDEN=1` hides the window. `SR_NATIVE_SAMPLE=1` prints the main thread once a second in a smoke run.
- `SR_DEBUGIT=1` shows the editors in the main menu. The smoke driver sets it.

Compare the native executable with the first one on the same scenarios:

```powershell
python -I instrumentation/client/smoke.py build/hook/opensubrosa24_client_native_smoke.exe menus level --compare <other smoke exe>
```

`menus` and the level editor give 0.0 percent of different pixels. The 3D scene of the demo differs a little:
the color of some cars is random.

Find a layout bug in this order:

1. Run the guard executable. A `GUARD <name>` line names the object and the function that wrote past its end.
2. Give the object its real size in `group_globals_*.csv`, or add it to `native_slack.csv`.
3. Run `python -I tools/re/client_overlap.py` and the address and start-value tests of the generator.

The native executable has no entry hook in its functions. The smoke driver finds the running frame loop
(game, building editor, level editor, model viewer) from the stack of the main thread.
`SR_SAMPLE=1` prints the position of the main thread once a second. The smoke driver clears `SR_SAMPLE`:
use `SR_NATIVE_SAMPLE=1` for a smoke run.
