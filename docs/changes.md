# Changes of behavior

This repository keeps the behavior of the vanilla game. This page lists each change that alters a result. A change that does not alter a result is not listed.

| Date | Where | What changed | Why |
| --- | --- | --- | --- |
| 2026-11-25 | `src/client/input/debug.c` | The debug keys P, O and I now set `game.paused`, `game.step_once` and `game.slow_tick`. They wrote other fields before. | The old code wrote the wrong fields, so the keys did nothing. They work only with the debug flag and a host. |
