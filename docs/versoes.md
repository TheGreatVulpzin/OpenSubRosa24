# Binary versions

## Summary

| Binary | Build date | Role |
| --- | --- | --- |
| `client/subrosa.exe`, `subrosa.x64`, Mac | 2015-08-02 | ALPHA 24 client, used by the group. |
| `client/subrosadedicated.exe` | 2015-08-02 | Dedicated server from the same build as the client. |
| `blackhouser/subrosadedicated024d.exe` | 2015-09-05 | "0.24d" dedicated server of Black Houser. Target of the server manager. |

## Official versions downloaded (`original/vanilla/`)

| File | Format | Symbols | Use |
| --- | --- | --- | --- |
| `0.24c_Dedicated_Linux/.../subrosadedicated024c` | ELF x64 | **Yes** (461 functions) | Main source of names for the 024d. |
| `0.24b_Dedicated_Linux/.../subrosadedicated024b` | ELF x64 | **Yes** (461 functions) | Second source of names. |
| `0.24c_Dedicated_Windows/.../subrosadedicated024c.exe` | PE32, MSVC | No | Revision before the 024d, same compiler. Bridge to the 024d. |
| `0.24b_Dedicated_Windows/.../subrosadedicated024b.exe` | PE32, MSVC | No | |
| `0.24d_Dedicated_Windows/.../subrosadedicated024d.exe` | PE32, MSVC | No | **Identical to the Black Houser 024d** (same SHA-256). Black Houser runs the 024d unmodified. |
| `0.24b/` | Full client (Windows, Linux, Mac) | ELF and Mach-O with symbols | 24b client. |
| `0.25b_Dedicated_Linux/subrosadedicated025b` | ELF x64 | No (stripped) | Next version. |
| `0.25b/` | Full 0.25b client (Windows, Linux, Mac) | Only the i386 Mach-O (4573 symbols) | Client of Oct 2015. The Mac build is an extra name reference for the 024d. |
| `0.22/`, `0.22_Dedicated_Server/` | 0.22 client and server | Mach-O with symbols | Previous version. |

All versions of the SUB ROSA ALL package (0.01 to 0.38f): see `docs/versoes-todas.md`. There is no 0.24d for Linux or Mac.

## The group's client vs. the 024d server

Players join Black Houser (024d) with the group's client. A helicopter, a train and an AK-47 show up on the server. Source: the user's report on 2026-10-11.

Conclusion: the 08/02 client and the 024d use the same protocol. **Network compatibility must follow the pair 08/02 client + 024d server.**

## String differences between the two dedicated servers

The 024d lacks several strings that the 08/02 dedicated server has. This does **not** mean the features are missing.

| Only in the 08/02 dedicated server | Only in the 024d |
| --- | --- |
| Vehicle names `Turbo`, `Turbo S`, `Truck` | `/phone`, `/give`, `/list3` |
| Item names `AK-47`, magazines | `gametype=` |
| Debug labels `rotorrpm`, `rotormass` | Debug labels `bondsim`, `objectsim`, `packetbuild` |
| `data/city.sbc`, `data/block/%s.sbl` | `Goldmen Inc` + `%s open` |

Likely explanations, to confirm in the code:

- The server does not need display names. Only the client shows names.
- The debug labels (timing per stage) changed between builds.

The 024d code is smaller: 356 KB in 340 functions, against 470 KB in 468 functions in the 08/02 dedicated server. The 024d uses more inlining.

## Version number

The client stores the version in the global variable `currentversion` (ELF, address `0x79731c`). The value is **24**. The main menu shows `ALPHA 24`. The server list shows `Alpha N` for each server.

The `024d` server is version 24, revision "d". Client and server have the same version.

## Open items

- Compare the vehicle type table (`setupobjecttypes_*`) between the ELF and the 024d.
