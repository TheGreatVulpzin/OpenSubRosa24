# Recording on another PC

A recording gives the offline test (src/tests/harness.c) real calls of the server. Any
Windows PC can make one.

1. Copy the folder of the test server (`build/auto-server`). The harness needs the same
   `subrosadedicated024d.exe` as the recording (the copy from `tools/re/fixed_base.py`).
2. Put these files in that folder: `wsock32.dll` (from `build/hook`), `subvulpes.ini` and
   `start_server.bat` (from this folder).
3. Open `start_server.bat`. It moves the previous recording into `recordings\<date-time>`
   and starts the server.
4. Play. `subvulpes.ini` writes the snapshot of the game memory after 600 seconds, so play
   at least 10 minutes.
5. Close the server, then open `start_server.bat` again or move the files by hand. Send the
   folder `recordings\<date-time>` back.

`subvulpes.ini` runs only the original code (`original_only=1`). The hook DLL only records.

The recording has player names, chat and the server `config.txt`. Keep it in `build/`.
Never commit it and never post it in public.
