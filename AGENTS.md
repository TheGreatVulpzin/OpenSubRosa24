# AGENTS.md

Instructions for agent that is not Claude Code (e.g. Codex) and writes code for this repository.

Read `CLAUDE.md` first. Its rules apply, with the changes below.

## What you do

Write clean C for one step of a group plan. User gives plan file (`plans/group<N>.md` in the package) and step number. Each plan step lists functions, names to add, rounding points, NaN compare forms, dead code, quirks to keep.

1. Read Names section and section of your step. Read `plans/names-8.md` for groups 8A, 8B, 8C. Never give a second name to an offset that `analysis/structs/names.csv` already names.
2. Add names to `analysis/structs/names.csv` (or `tools/re/gen_types.py` for custom type), then run `python -I tools/re/gen_types.py`.
3. Write clean C in `src/game`, `src/common` or `src/net`, as plan says. Remove the `lf_XXXXX` alias of each function from its callers. Follow comment rules of `CLAUDE.md` (step 3 of workflow).
4. If you have clang, check syntax: `clang -m32 -fsyntax-only -Isrc/include -Isrc/common -Isrc/game -Isrc/net <file>`. This check proves nothing about the result.
5. Make one commit per step, in English. Do not push. Message names group and step. Report lists functions, new names, each place where you were unsure of a rounding point.

## What you do not do

- Gate needs game binaries, MSVC build and recordings, not in repository. Machine has them: run `bash tools/lift/step.sh [--rec <recording dir>] [--synth <name>,...] <function>,...` before the commit, put summary in report. Machine lacks them: user runs `step.sh` on your commit. Float difference in last bits = most common failure: follow each rounding point of plan exactly.
- Do not edit `src/lifted/`, `hook/hooks_lifted.c` or generated tables by hand. `tools/lift/pipeline.sh` regenerates them. Run it, commit result with the step (see checklist below).
- Do not change lifted-style files (`src/game/objects.c`, `wheels.c`, `ai_car.c`) unless plan says so.
- Ignore parts of `CLAUDE.md` about Claude sessions: caveman, cavecrew, subagents, model choice, plan sessions, review by another model.
- Never commit anything in `original/` or `build/`, never a recording or a `config.txt`.

## End of a step (checklist)

Do every item before reporting "done". Show proof of each item in report. Paste output of `git status` and `step.sh` summary.

1. Run `bash tools/lift/pipeline.sh`. Search its log for `error C` and `build failed`. Script exits 0 when build fails.
2. Run `git status`. After lift, `src/lifted/lifted_024d.c`, `src/lifted/lifted_table.c` and `hook/hooks_lifted.c` must show changes. Commit them with the step.
3. Run `bash tools/lift/step.sh <functions>`. Summary must name each function of the step. Missing function = not tested.
4. Run `python -I tools/lift/bench.py` for each function that runs every tick.
5. Run `grep -n -i "original\|lifted\|rewritten\|024d" <files you wrote>`. Must find nothing.
6. Docs and `names.csv` must agree. Field meaning changed: `grep` its name in `docs/`, fix each hit.
7. Tell user step needs review by another model. Step with float math needs strong model.
8. After commit, `git status` must be clean. No push.

Machine lacks game binaries or recordings: do items 1 to 4 as far as possible, say in report which items you could not do.

## Which steps

Take steps with little float math first: tables (`setup*`), getters, setters, file reads and writes, copies. Leave steps with heavy float math (vehicle physics, collision, bezier surfaces, `buildsomeshit`, `building_buildfloorplan`) to user, unless user asks you to do one.
