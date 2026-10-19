#!/usr/bin/env bash
# The whole lift pipeline in one command (Windows, Git Bash): Ghidra export, lift, build, gate.
#
# Usage: bash tools/lift/pipeline.sh [--export] [--params] [--no-gate] [--full] [gate.py options]
#   --export   run LiftExport.java in Ghidra first. Without it, lift.py reads the export that is
#              already in build/lift/ded024d.jsonl. The lifted C has a frozen header
#              (src/lifted/sr_types_lifted.h): a new export after an ApplyTypes run gives it new
#              names, so freeze the header again then.
#   --params   run ApplyParams.java first (after a change to analysis/024d/params.csv).
#              It writes to the Ghidra project: make a backup first if the change is large.
#   --no-gate  stop after the build.
#   --full     full gate (all recordings, all functions, about 3 min). Use it before a push.
#              Without it the gate is "--changed --quick" (about 1 min).
# Other options go to tools/lift/gate.py (for example --update, --only murilo).
#
# Steps: [ApplyParams.java] -> [LiftExport.java (5 min)] -> lift.py --compile (2-5 min)
#        -> tools\build_hook.bat -> gate.py (about 1 min, --full about 3 min). Heavy: run nothing else meanwhile.
# The script stops at the first step that fails. The gate fails when a "calls same" count drops.
set -euo pipefail
source "$(dirname "$0")/../ghidra/env.sh"
cd "$ROOT"

do_export=0 params=0 gate=1 full=0 gate_args=()
for a in "$@"; do
    case "$a" in
        --export) do_export=1 ;;
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

if [ $do_export = 1 ] || [ $params = 1 ] || [ ! -f "$ROOT/build/lift/ded024d.jsonl" ]; then
step "LiftExport.java"
out=$("$HEADLESS" "$PROJECT" subvulpes -process subrosadedicated024d.exe -noanalysis -readOnly \
    -scriptPath "$ROOT/tools/ghidra" -postScript LiftExport.java "$ROOT/build/lift/ded024d.jsonl" 2>&1)
echo "$out" | grep -E "Exported|ERROR|Exception" || true
echo "$out" | grep -q "Exported:" || { echo "LiftExport failed"; exit 1; }
fi

step "lift.py --compile"
python -I tools/lift/lift.py --compile | grep -E "^result|^top reasons"
git diff --stat -- src/lifted hook/hooks_lifted.c

step "build_hook.bat"
build=$(cmd //c "tools\\build_hook.bat" 2>&1) || true
echo "$build" | grep -E " error C| error LNK" | head -10 || true
echo "$build" | grep -q "^OK:" || { echo "build failed"; exit 1; }
echo "$build" | tail -1

if [ $full = 0 ]; then
    gate_args=(--changed --quick "${gate_args[@]}")
fi
if [ $gate = 1 ]; then
    step "gate.py ${gate_args[*]:-}"
    python -I tools/lift/gate.py "${gate_args[@]}"
fi
step "done"
