#!/usr/bin/env bash
# The whole lift pipeline in one command (Windows, Git Bash): Ghidra export, lift, build, gate.
#
# Usage: bash tools/lift/pipeline.sh [--params] [--no-gate] [--full] [gate.py options]
#   --params   run ApplyParams.java first (after a change to analysis/024d/params.csv).
#              It writes to the Ghidra project: make a backup first if the change is large.
#   --no-gate  stop after the build.
#   --full     full gate (all recordings, all functions, 30-40 min). Use it before a push.
#              Without it the gate is "--changed --quick" (about 1-5 min).
# Other options go to tools/lift/gate.py (for example --update, --only murilo).
#
# Steps: [ApplyParams.java] -> LiftExport.java (5 min) -> lift.py --compile (2-5 min)
#        -> tools\build_hook.bat -> gate.py (1-5 min, --full 30-40 min). Heavy: run nothing else meanwhile.
# The script stops at the first step that fails. The gate fails when a "calls same" count drops.
set -euo pipefail
source "$(dirname "$0")/../ghidra/env.sh"
cd "$ROOT"

params=0 gate=1 full=0 gate_args=()
for a in "$@"; do
    case "$a" in
        --params) params=1 ;;
        --no-gate) gate=0 ;;
        --full) full=1 ;;
        *) gate_args+=("$a") ;;
    esac
done

step() { echo; echo "== $(date +%H:%M:%S) $*"; }

if [ $params = 1 ]; then
    step "ApplyParams.java"
    "$HEADLESS" "$PROJECT" subvulpes -process subrosadedicated024d.exe -noanalysis \
        -scriptPath "$ROOT/tools/ghidra" -postScript ApplyParams.java "$ROOT/analysis/024d/params.csv" 2>&1 |
        grep -E "ApplyParams.java>|ERROR|Exception" || true
fi

step "LiftExport.java"
out=$("$HEADLESS" "$PROJECT" subvulpes -process subrosadedicated024d.exe -noanalysis -readOnly \
    -scriptPath "$ROOT/tools/ghidra" -postScript LiftExport.java "$ROOT/build/lift/ded024d.jsonl" 2>&1)
echo "$out" | grep -E "Exported|ERROR|Exception" || true
echo "$out" | grep -q "Exported:" || { echo "LiftExport failed"; exit 1; }

step "lift.py --compile"
python -I tools/lift/lift.py --compile | grep -E "^result|^top reasons"
git diff --stat -- src/lifted hook/hooks_lifted.c

step "build_hook.bat"
cmd //c "tools\\build_hook.bat" | tail -1

if [ $full = 0 ]; then
    gate_args=(--changed --quick "${gate_args[@]}")
fi
if [ $gate = 1 ]; then
    step "gate.py ${gate_args[*]:-}"
    python -I tools/lift/gate.py "${gate_args[@]}"
fi
step "done"
