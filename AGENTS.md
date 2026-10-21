# AGENTS.md

Instructions for an agent that is not Claude Code (for example Codex) and that writes code for this repository.

Read `CLAUDE.md` first. Its rules apply to you, with the changes below.

## What you do

You write clean C for one step of a group plan. The user gives you the plan file (`plans/group<N>.md` in the package) and the step number. Each step of a plan lists its functions, the names to add, the rounding points, the NaN compare forms, the dead code and the quirks to keep.

1. Read the Names section and the section of your step. Read `plans/names-8.md` for groups 8A, 8B and 8C. Do not give a second name to an offset that `analysis/structs/names.csv` names already.
2. Add the names to `analysis/structs/names.csv` (or to `tools/re/gen_types.py` for a custom type), then run `python -I tools/re/gen_types.py`.
3. Write the clean C in `src/game`, `src/common` or `src/net`, as the plan says. Remove the `lf_XXXXX` alias of each function from its callers. Follow the comment rules of `CLAUDE.md` (step 3 of the workflow).
4. If you have clang, check the syntax: `clang -m32 -fsyntax-only -Isrc/include -Isrc/common -Isrc/game -Isrc/net <file>`. This check proves nothing about the result.
5. Open one pull request for each step. Its title names the group and the step. Its text lists the functions, the new names, and each place where you were not sure of a rounding point.

## What you do not do

- The gate needs the game binaries, the MSVC build and the recordings, which are not in the repository. When your machine has them, run `bash tools/lift/step.sh [--rec <recording dir>] [--synth <name>,...] <function>,...` before you open the pull request, and put its summary in the text. When it does not have them, the user runs `step.sh` on your pull request. A float difference in the last bits is the most common failure: follow each rounding point of the plan exactly.
- Do not change `src/lifted/`, `hook/hooks_lifted.c` or the generated tables. The user regenerates them with `tools/lift/pipeline.sh`.
- Do not change the lifted-style files (`src/game/objects.c`, `wheels.c`, `ai_car.c`) unless the plan says so.
- Ignore the parts of `CLAUDE.md` about Claude sessions: caveman, cavecrew, subagents, the choice of model, the plan sessions and the review by another model.
- Never commit anything in `original/` or `build/`, and never a recording or a `config.txt`.

## Which steps

Take the steps with little float math first: tables (`setup*`), getters, setters, file reads and writes, copies. Leave the steps with heavy float math (vehicle physics, collision, bezier surfaces, `buildsomeshit`, `building_buildfloorplan`) to the user, unless the user asks you to do one.
