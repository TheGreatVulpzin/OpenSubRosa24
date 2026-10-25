# Security policy

## Report a problem

Do not open a public Issue for a security problem.

1. Open the *Security* tab of this repository.
2. Choose *Report a vulnerability*.
3. Say what you found, how to repeat it and what it does.

## What counts

- A packet from a client that crashes the server, freezes it or writes outside a buffer.
- A data file (`.sbb`, `.sbl`, `city.sbc`, `server.srk`, `readme.txt`) that crashes the server or writes outside a buffer.
- A way to use an admin command without the password.

## Versions

Only the `main` branch gets fixes.

## Known problems

Some problems are known and have a fix planned. The list is in [docs/ideas.md](docs/ideas.md) (section "Security fixes") and in [docs/protocol.md](docs/protocol.md) (section 12).

## Do not post private data

Never post passwords, `config.txt`, recordings or files of the original game.
