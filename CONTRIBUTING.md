# Contributing

Bug reports and pull requests are welcome.

Download the server package from [Releases](https://github.com/TheGreatVulpzin/OpenSubRosa24/releases). See [the release process](docs/release.md).

## Bug reports

Open an Issue. Say what you did, what you expected and what happened. Add the server log.

## Pull requests

- For a big change, open an Issue first.
- Write code, comments and docs in English.
- Keep comments short, please. Say why the code does something, not what the code already shows.

## What a change may do

This repository gives the source of the game with the vanilla behavior: the protocol, the simulation and the result of every function. New features go to other projects (Overrosa for the client, SubVulpes for the server).

A change here is welcome when it is one of these:

- a security fix or a crash fix;
- a build fix;
- a gain in speed that keeps every result bit for bit;
- a gain in readability.

A change that alters a result is rare. Its commit says what changed and why, and `docs/changes.md` lists it.

## Build and test

The client (Windows, x86). The result is `build\hook\opensubrosa24_client_native.exe`:

```bat
tools\build_native.bat subvulpes-client-native
bash tools/native_test.sh --hidden menus level
```

The test runs the client in a hidden window and fails when a scenario fails or the build is old. Add `--guard` to find a write past the end of a data object.

The server:

```bat
tools\build_server.bat
bash instrumentation/lift/pipeline.sh --full
```

## The data files

The data of the game is plain source, one file for each system: `src/client/data/*.c`, `src/server/data/*.c` and their headers. Edit them like any other file. The type headers (`src/include/sr_types.h` and its siblings) come from `tools/re/gen_types.py`: change a name or a field in `analysis/structs/names.csv`, then run the tool.

## It must be bit-exact

The results of the game do not change unless `docs/changes.md` says so. Fixes for invalid input are welcome.

## Gate

The server gate (the second command above) takes about 3 minutes. It needs the original game files and test recordings that are not in this repository. If you cannot run it, say so in the pull request, and the maintainer will run it.

## Security problems

Do not open a public Issue. Read [SECURITY.md](SECURITY.md).

## Private data

Never post passwords, `config.txt`, recordings or files of the original game.
