# Contributing

Bug reports and pull requests are welcome.

## Bug reports

Open an Issue. Say what you did, what you expected and what happened. Add the server log.

## Pull requests

- For a big change, open an Issue first.
- Write code, comments and docs in English.
- Keep comments short, please. Say why the code does something, not what the code already shows.

## It must be bit-exact

This project gives the same results as the original game.
Fixes for invalid input are welcome.

## Gate

Run the gate before you open a pull request:

```bash
bash instrumentation/lift/pipeline.sh --full
```

The gate takes about 3 minutes. It needs the original game files and test recordings that are not in this repository. If you cannot run it, say so in the pull request, and the maintainer will run it.

## Security problems

Do not open a public Issue. Read [SECURITY.md](SECURITY.md).

## Private data

Never post passwords, `config.txt`, recordings or files of the original game.
