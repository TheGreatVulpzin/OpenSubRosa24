#!/usr/bin/env bash
# One cycle for the client: lift, generated tables, build, gate.
#
# Usage: bash instrumentation/client/pipeline.sh [--export] [--full] [--no-gate] [--update]
#   --export   run LiftExport in Ghidra first (names, types or LiftExport.java changed)
#   --full     run the gate on every function (default: only the functions whose lifted C changed)
#   --no-gate  stop after the build
#   --update   write the baseline after the gate
# Run it in the background: it takes minutes. The log of each step is in build/pipeline_client/.
set -uo pipefail
cd "$(dirname "$0")/../.."
export ROOT="$PWD"
LOG=build/pipeline_client
mkdir -p "$LOG"

export_step=0; gate_args="--changed"; run_gate=1; update=""
for a in "$@"; do
    case "$a" in
        --export) export_step=1 ;;
        --full) gate_args="" ;;
        --no-gate) run_gate=0 ;;
        --update) update="--update" ;;
        *) echo "unknown option: $a"; exit 2 ;;
    esac
done

step() { echo "== $(date +%H:%M:%S) $1"; }

if [ "$export_step" = 1 ]; then
    step "LiftExport"
    source instrumentation/ghidra/env.sh
    "$HEADLESS" "$PROJECT" subvulpes -process subrosa.exe -noanalysis -readOnly \
        -scriptPath "$ROOT/instrumentation/ghidra" \
        -postScript LiftExport.java "$ROOT/build/lift/client_win.jsonl" > "$LOG/export.log" 2>&1
    if grep -q "ERROR.*error:" "$LOG/export.log"; then
        echo "FAILED: LiftExport did not compile, see $LOG/export.log"
        exit 1
    fi
fi

step "lift"
python -I instrumentation/lift/lift.py --target client_win --compile > "$LOG/lift.log" 2>&1
grep "^result:" "$LOG/lift.log"

step "tables"
python -I instrumentation/client/gen_count_table.py
python -I instrumentation/client/gen_plat_stubs.py

step "build"
cmd //c "tools\\build_hook.bat" > "$LOG/build.log" 2>&1
if grep -q "error C\|error LNK\|fatal error" "$LOG/build.log"; then
    echo "FAILED: build, see $LOG/build.log"
    grep -m5 "error C\|error LNK\|fatal error" "$LOG/build.log" | cut -c1-200
    exit 1
fi

if [ "$run_gate" = 1 ]; then
    step "gate"
    python -I instrumentation/lift/gate.py --target client_win --jobs 2 $gate_args $update | tee "$LOG/gate.log" | tail -12
fi
step "done"
