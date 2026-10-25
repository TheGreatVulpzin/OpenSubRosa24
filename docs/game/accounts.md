# Accounts

An account keeps the money and the looks of a player between games. The server saves all accounts in `user/server.srk`. A player joins with two ids. The server finds the account by these ids. The file format is in [formatos.md](../formatos.md).

Only the load function is in clean C: `src/server/accounts.c`. The save, create and lookup functions still run as lifted C.

## Records

| Record | Key fields | Use |
| --- | --- | --- |
| `Account` (0x4c bytes) | `id[2]`, `name[32]`, `name_locked`, `money`, `look_a`, `look_b`, `look_c` | One account. `id` equals `Player.account_id`. |
| `account` (global) | 32-bit key | A random key. The file stores it. |
| `numofaccounts` (global) | int | The number of accounts. |
| `accounts` (global) | array of `Account` | The account table. It starts 8 bytes after the key. |

Field notes:

- `name_locked` does not go to the file. `createaccount` writes 5 to it.
- A new account has 1000 money. The money can go below 0 (a debt).
- `look_a` is `rand() % 3`. `look_b` is `rand() & 7`. `look_c` is `rand() & 3`.
- In memory the record is 0x4c bytes. In the file it is 60 bytes.

## Load flow

`sr_loadaccountsserver` runs once when the server starts.

1. It sets the key to a new random word (`genrand_int32`). The key stays new when the file is missing.
2. It sets the count to 0.
3. It opens `user/server.srk`. When the file is missing, it returns.
4. It reads the version. The game reads versions 1 to 3. It ignores other versions.
5. It reads the key and the count from the file.
6. For each account it reads the two ids, the 32 name bytes and the money.
7. For version 2 and 3 it reads one unknown int and the three looks.
8. It closes the file.

The loader accepts counts from 0 through 65535 and checks each read. Each name ends with zero at index 31.
The loader applies account records and the saved key only after all reads pass.
Invalid files give no accounts and keep the new key.

Version rules:

- Version 1 has no looks. The game rolls the three looks with `rand()`.
- Version 2 rolls the looks again when all three are 0.
- Version 3 uses the looks from the file.

The load code calls the C library of the game for `fopen`, `fclose` and `rand`. It uses the `fread` pointer that the game sets. The state of `rand()` is the state of the game CRT.

## Use of accounts in other systems

- A dead human without a player costs its account 5 percent of the money above 10000 (`human_releaseplayer` in `humans.c`).
- `createplayer` and `createaccount` find or make an account from the two ids of a player.

## Functions

| Function | RVA | File | Purpose |
| --- | --- | --- | --- |
| `sr_loadaccountsserver` (`loadaccountsserver`) | 0xe2e0 | accounts.c | Read the accounts from `user/server.srk`. |

Functions of the system that are not rewritten yet:

| Function | RVA | Purpose |
| --- | --- | --- |
| `account_nameexists` | 0x1010 | Test if an account name is in use. |
| `saveaccountsserver` | 0x1080 | Write `user/server.srk` as version 3. |
| `createaccount` | 0xded0 | Find the account of a player or make a new one. |
| `saveaccountsclient` | 0x11d0 | Write `client.srk`. |
| `loadaccountsclient` | 0x12f0 | Read `client.srk`. |

## Known odd behavior

- The original reads an uninitialized version when the file is empty. The clean code reads 0 and ignores the file.
- The game writes all 32 name bytes to the file. The bytes after the first 0 are not clean.
