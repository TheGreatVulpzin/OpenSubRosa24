# Standalone client smoke tests

Use a Windows desktop with a display and OpenGL. Run one client at a time.

```powershell
python -I instrumentation/client/smoke.py build/client_test/opensubrosa24_client_smoke.exe
python -I instrumentation/client/smoke.py build/client_test/opensubrosa24_client_smoke.exe menus buildings
```

Build the test client with `cmd //c "tools\\build_client.bat"` in Git Bash.
Copy `build/hook/opensubrosa24_client.exe` to `build/client_test/opensubrosa24_client_smoke.exe`.
Keep the other executables for comparison.

The executor uses `build/client_test` as the working folder.
Each scenario uses a new process with `SR_SMOKE=<scenario>`, `SR_DEBUGIT=1` and `SR_TRACE=3`.

- `menus`: main menu, Options, Voice Options, Start (host) and Join. Each screen runs for at least 5 seconds. Back returns to the main menu.
- `buildings`: select each `data/building/*.sbb` file. Enter the game, press F1, render 120 editor frames, exit, and select the next file.
- `level`: enter Demo, press F1, render 120 editor frames, and exit.
- `model`: use the debug shortcut A, render 120 viewer frames, and exit.

The driver reads menu labels and hit boxes. It scales the 1024 by 640 menu coordinates to the window pixels.
It sends SDL mouse and key events and calls `SDL_WarpMouseInWindow`.
SDL events do not change the state returned by SDL polling calls.
Test wrappers supply matching mouse and key state only when `SR_SMOKE` is set.
Frame counts come from completed window swaps. The test does not write game records or call editor loops directly.

Each wait has a time limit. The driver sends `SDL_QUIT` after the checks.
A pass needs the READY and PASS markers and exit code 0.
A crash, timeout, missing driver or incomplete run fails.
The executor stops after two timeouts at the same stage.
The executor rejects an executable without this driver before it starts a process.

Logs and `summary.json` go to `build/client_smoke/<exe name>/`.
Use `--output <folder>` to keep a separate run or `--timeout <seconds>` to change the process limit.
The summary lists the scenario, result and last error line. Exit code 0 means all scenarios pass.
`SR_TRACE=3` adds recent calls and the module name after a crash.
Test settings use `build/client_test/Sub Rosa/`. The driver redirects the documents path only for a smoke run.
The executor restores the configuration, building files and city file after each process.
The tests check input paths and frame progress. They do not check pixel colors, sound quality or network play.
