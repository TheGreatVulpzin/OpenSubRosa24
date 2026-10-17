# Progress

How much of `subrosadedicated024d.exe` runs in C. Generate the table again with:

```bash
python -I tools/progress.py --md
```

The function list and the sizes come from `analysis/024d/functions.csv` (340 functions, 356,035 code bytes). The harness lines come from the last full gate (`build/gate/ded024d/baseline.json`, 10 recordings).

## Phase 3: 2026-10-16

| Measure | Functions | Code bytes |
| --- | --- | --- |
| Functions in C (hand-written) | 5 of 340 (1.5%) | 1142 of 356035 (0.32%) |
| Functions in C (lifted, live) | 277 of 340 (81.5%) | 283163 of 356035 (79.53%) |
| Functions in C (lifted, harness only) | 39 of 340 (11.5%) | 68922 of 356035 (19.36%) |
| Functions in C (total) | 321 of 340 (94.4%) | 353227 of 356035 (99.21%) |
| Functions verified in the game | 3 of 340 (0.9%) | 580 of 356035 (0.16%) |
| Functions in C with a "same" call | 284 of 340 (83.5%) | 325487 of 356035 (91.42%) |
| ... and no difference on any recording | 283 of 340 (83.2%) | 316662 of 356035 (88.94%) |

How to read the table:

- **Live:** the lifted function replaces the original in the running server (`hook/hooks_lifted.c`).
- **Harness only:** the function has side effects (it sends packets or writes files). The live server keeps the original. The offline harness runs the lifted body in the test environment (`docs/side-effects.md`).
- **Verified in the game:** a line marked **Verified** in `docs/hooks.md`. Only the first hand-written hooks have this mark. The in-game tests of 2026-10-15 (`own_main=1`, all lifted hooks, 15 min with players, no crash) are not in this count.
- **No difference:** `building_buildfloorplan` is out of this line because of one harness-only difference (`docs/lift.md`). It is "same" in reset mode. `buildsomeshit` has no "same" call: its x87 difference is accepted (`docs/lift.md`).

## What is not in C

The 19 functions that are not in C:

- `getpaths` (the real `main`) and `dedicatedserverloop`. `src/server/main_loop.c` has a hand-written version of the loop (`own_main=1`). It is not a hook, so the table does not count it.
- 17 functions of the MSVC CRT. Phase 4 links its own CRT and does not need them.

32 of the 340 functions are not referenced by the live code (client or editor code, `docs/phase4-inventory.md`). 13 of them have hand-written harness calls (`tools/recording/synth_calls.txt`).

## Functions without a harness call

56 functions have no "same" call in the gate baseline (2026-10-17). No live function is in this list, so new recordings are not necessary for the function count. Record again only when a rewrite changes a code path that no recording covers.

| Group | Count | Functions |
| --- | ---: | --- |
| Dead (`†` in `docs/phase4-inventory.md`) | 17 | client accounts, `building_save`, `leveleditor_save`, `clientrecv`, `unpackobjectpacket`, `packobjectpacket2`, `object_rotorsimulation`, `logicsimulation_driving` and 8 small helpers |
| Dead through a dead caller, or no caller in the C | 14 | `unpackevent`, `unpackorientation`, `unpackangle`, `pbitreadnego` (callers: `clientrecv`, `unpackobjectpacket`), `economysimulation`, `init_by_array`, `testtimer`, `packunitvector`, `packscalar`, `packpositiondelta`, `unpackpositiondelta`, `lerpnoise2d`, `lineintersectline2d`, `sphereintersectline2d` |
| CRT and `main` (not in C) | 20 | `getpaths`, `dedicatedserverloop` and 18 CRT functions |
| Hand-written in `src/standalone/fn_table.c` | 4 | `createevent_message`, `createevent_updatedoor`, `createevent_updatemission`, `fixplayername` |
| Accepted difference (`docs/lift.md`) | 1 | `buildsomeshit` |

## Phase 4: what the lifted C still takes from the image

`python -I tools/lift/deps.py` on 2026-10-16:

| Dependency | Count |
| --- | ---: |
| Calls to the `.exe` | 0 |
| Different globals | 1473 (3444 references) |
| Inline `SR_VA` (strings and constants) | 657 |
| Import references | 123 |
