# Recording on another PC

A recording gives the offline test (instrumentation/tests/harness.c) real calls of the server. Any
Windows PC can make one.

1. Copy the folder of the test server (`build/auto-server`). The harness needs the same
   `subrosadedicated024d.exe` as the recording (the copy from `tools/re/fixed_base.py`).
2. Put these files in that folder: `wsock32.dll` (from `build/hook`), `record_focus.txt`,
   `record_normal.bat`, `record_subvulpescode.bat` and `save_last_record.bat` (from this
   folder).
3. Open one of the two `record_*.bat` files:
   - `record_normal.bat`: the original game code runs (`original_only=1`). The hook DLL
     only records.
   - `record_subvulpescode.bat`: the SubVulpes C code runs instead of the original: the
     hand-written main loop (`own_main=1`) and the lifted functions (`original_only=0`).
     This is the in-game test of the C code, and it records at the same time.
   The game mode (World, Round) comes from `config.txt`. Change it there by hand.
4. Play at least 5 minutes: the snapshot of the game memory is written after 300 seconds.
5. Close the server window. The `.bat` window waits for the server, then
   `save_last_record.bat` moves the files into `recordings\<date-time>-<mode>` and makes a
   `.zip` of that folder. Send the `.zip` back.

If the `.bat` window was closed before the server (or the PC crashed), open
`save_last_record.bat`: it saves the files that were left. A `record_*.bat` file also saves
them first, before it starts the server.

Each `record_*.bat` file writes `subvulpes.ini` for its mode, so do not edit that file.

The recording has player names, chat and the server `config.txt`. Keep it in `build/`.
Never commit it and never post it in public.
