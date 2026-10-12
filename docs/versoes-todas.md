# All versions (SUB ROSA ALL package)

## Source

- Package: `SUB ROSA ALL.rar`, from `https://archive.org/download/sub-rosa-all`.
- Size: 2,222,098,574 bytes. SHA-256 `9198f70edbb35e3e44f8918a4307dc1f018904731242a33e6efcc12a7bda2fdf`.
- Put together by "Jenerick". It has 99 packages, from 0.01 to 0.38f.
- Processed in the cloud on 2026-10-12. The files stayed outside the repository (`~/sr-all/`).

The full table (303 executables, no duplicates) is in `analysis/versoes/executaveis.tsv`.
To generate it again: `python -I tools/re/survey_bins.py <extracted folder> --manifest original/MANIFEST.sha256 --tsv`.

## Main conclusion

- **There is no 0.24d for Linux or Mac in the package.** The 0.24 series has only the same files we already have in `original/vanilla/`.
- The 0.24d exists only as `subrosadedicated024d.exe` (PE32, no symbols). The hash is the same as Black Houser's.
- The binary with symbols closest to the 024d **after** it is the 0.25 i386 Mac client (October 2015, 4573 symbols).
- Before the 024d, the closest are still the 0.24b/0.24c Linux servers (461 functions) and the 0.24b client.

## Versions and symbols

"Build" is the date in the PE header (TimeDateStamp). ELF and Mach-O do not have this date.

| Version | PE build | Windows (PE) | Linux (ELF) | Mac (Mach-O) |
| --- | --- | --- | --- | --- |
| 0.01 – 0.05 | 2012-06 | `subrosa.exe` (VC6), no symbols | – | – |
| 0.06, 0.06b | 2012-08 | `subrosa.exe` (VC6) | `subrosa` x64 **2433 functions**; `subrosadedicated32/64` **296 functions** | – |
| 0.07, 0.07b | 2012-09/10 | `subrosa.exe` (VC6) | `subrosadedicated32/64` **321 functions** | `subrosa` i386 **4350 symbols** |
| 0.15 test | 2014-02-26 | `subrosa.exe`; unofficial dedicated server (marek9911) | – | – |
| 0.21 | 2014-08-14 | `subrosa.exe`; unofficial dedicated server (marek9911) | – | i386 **4502 symbols** |
| 0.22 | 2014-10-31 | `subrosa.exe`; unofficial dedicated server (mordekai) | – | i386 **4503 symbols** |
| 0.23 | 2015-01-01 | `subrosa.exe`; unofficial dedicated server (marek9911) | `subrosa64` **2681 functions** | i386 **4539 symbols** |
| 0.24b | 2015-08-02/04 | `subrosa.exe`, `subrosadedicated.exe`, `subrosadedicated024b.exe` | `subrosa.x64` **2757**; `subrosadedicated024b` **461** | i386 **4662 symbols** |
| 0.24c | 2015-08-04 | `subrosadedicated024c.exe` | `subrosadedicated024c` **461 functions** | – |
| 0.24d | 2015-09-05 | `subrosadedicated024d.exe` | – | – |
| 0.25, 0.25b | 2015-10-06 | `subrosa.exe`, `subrosadedicated025b.exe` | `subrosa.x64` and `subrosadedicated025b` without symbols | i386 **4573 symbols** |
| 0.25 patches | 2015-10-16/18 | `subrosatest_b` … `_g.exe` | – | – |
| 0.25-1 | 2015-10-06 | `subrosa.exe` (same as 0.25) | `subrosa.x64` without symbols | x86-64 **4570 symbols** |
| 0.25c, 0.25d | 2017-03/05 | `subrosa.exe` | `subrosa.x64` without symbols | x86-64 **4570 symbols** (same as 0.25-1) |
| 0.26a | 2016-08-31 | `subrosa.exe` (Traffic Test) | – | – |
| 0.27a | 2017-08-02 | `subrosa.exe` | – | – |
| 0.28a, 0.28b | 2017-09 | `subrosa.exe` | – | – |
| 0.29a | 2017-12/2018-01 | `subrosa.exe` | `subrosa.x64`, `subrosadedicated028.x64` without symbols | x86-64 1903 symbols |
| 0.30a Test 9 | 2018-08-08 | `subrosa.exe` | – | – |
| 0.31 – 0.35 | 2018-09 to 2019-06 | client and dedicated server | client and dedicated server without symbols | x86-64 1942 to 2044 symbols |
| 0.36 betas (v, x, y, z) | 2019-09/10 | client and dedicated server | without symbols | x86-64 2044 symbols |
| 0.36a, 0.36b | 2019-10 | client and dedicated server | without symbols | – (no Mac) |
| 0.37 | 2020-06 to 2020-08 | client and dedicated server | without symbols | – |
| 0.38 | 2021-03 to 2021-09 | PE32+ x64, MSVC 14.26 | without symbols | – |

Notes:

- All PE files from 0.15 to 0.37 are i386 with linker 10.0 (MSVC 2010). 0.38 moves to x64 and MSVC 2019.
- No PE has a COFF table or a PDB path.
- Since 0.25, Linux ships **stripped**. Only the Mac keeps symbols.
- From 0.29 on, the Mac has fewer symbols (~1900–2044). But it still has game function names (`_gameloop`).
- The dedicated servers 0.15, 0.21, 0.22 and 0.23 are **unofficial** (made by the community).

## Files we already had

Comparison with `original/MANIFEST.sha256`. Equal hashes:

| File in the package | Already in |
| --- | --- |
| 0.22 `subrosa.exe` and Mac `subrosa` | `vanilla/0.22/` |
| 0.24b client (`subrosa.exe`, `subrosa.x64`, Mac, `subrosadedicated.exe`) | `client/` and `vanilla/0.24b/` |
| 0.24b and 0.24c dedicated servers (Linux and Windows) | `vanilla/0.24b_*`, `vanilla/0.24c_*` |
| 0.24d `subrosadedicated024d.exe` | `blackhouser/` and `vanilla/0.24d_Dedicated_Windows/` |
| 0.25b `subrosadedicated025b` (Linux) | `vanilla/0.25b_Dedicated_Linux/` |
| 0.25/0.25b client (`subrosa.exe`, `subrosa.x64`, Mac i386) | `vanilla/0.25b/` (downloaded in this session) |

The 0.25 `subrosa.x64` is the same up to 0.25d. Everything else is new to the project.

## New binary in `original/`

| File | Source | SHA-256 |
| --- | --- | --- |
| `vanilla/0.25b/Sub Rosa 0.25b/subrosa.app/Contents/MacOS/subrosa` | `0.25/b/Sub Rosa 0.25b.zip` from gart.sh. It is the same binary as the 0.25 in the package. | `771dc2ab…c099` |

`tools/cloud/setup.sh` now downloads this package (`vanilla/0.25b/`).
In the Ghidra project it sits in the folder `/mac025`. Reason: the name `subrosa` already belongs to the Mac 0.24b.
Short name of the features: `mac025_i386`.

## Effect on the 024d names

The Mac 0.25 became a `--ref` in the 024d step of `tools/re/name_all.sh`.
Both runs started from the same `functions.csv` (the one from the previous commit).

| Run | 024d functions with a name |
| --- | --- |
| Without the Mac 0.25 | 189 of 340 |
| With the Mac 0.25 | 194 of 340 |
| With the Mac 0.25 + manual `playerai` | **195 of 340** |

Changes:

- New names: `city_addintersection`, `serpnoise2d`, `addbeziersurfacetexture`, `lineintersectarea`, `createevent_sound`, `playersimulation` (0xa2a0).
- `objecttype_addparticle` moved from 0xb0b0 to 0x28a90 (18 → 96 votes; size closer to the ELF).
- 0xb540 changed from `buildcitystreet` to `setuptimers`. The size (381 bytes, no calls) matches `setuptimers` (436 in the ELF). `buildcitystreet` has 4457 bytes.
- `lineintersectlevel` moved from 0x21b40 to 0x4e500. Low confidence (0.333). Needs checking.
- The Mac 0.25 changed 0x4f720 from `playerai` to `playersimulation`. **Wrong**: the function calls `ai_assessthreats` and `ai_playercar`. Fixed in `analysis/024d/manual_names.csv`.

Warning: in the Mac 0.25 the functions are much smaller than in 0.24b (e.g. `playerai` has 4091 bytes against 6497). The code changed between versions. Names that come only from it need checking.

## Other things in the package

- `Essential mods for versions before 0.25/`: an `SDL2.dll` for users with a Latin-language keyboard (to type `/` and `?` in chat, versions before 0.26). Also a `.txt` with a Discord link for the new master server patch.
- `readme.txt`: list of community Discord servers.
