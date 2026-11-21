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

Select scenarios after the executable path: `menus explore play crash buildings level model`.
The default runs all seven scenarios. Use `--timeout <seconds>` to set a process limit.

- `menus` opens Options, Voice Options, Start and Join. It captures each main screen.
- `explore` visits visible controls to depth three. It stops after 90 seconds, excluding client startup.
  It records button labels, field values and depth in `screens.json`.
  It types `smk` in string fields and `7` in numeric fields, then checks the field value.
  It skips Exit, Quit, Back, destructive actions, Apply, CPU affinity, web links and remote connection actions.
  Back returns to each parent. Escape returns when the screen has no Back button.
  A time limit or an unexpected parent fails the scenario. The log records omitted actions.
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