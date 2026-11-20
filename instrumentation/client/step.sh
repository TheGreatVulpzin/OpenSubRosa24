#!/usr/bin/env bash
# One step of the client rewrite: build, test the functions of the step, print a short summary.
#
# Usage: bash instrumentation/client/step.sh [--export] [--synth f1,f2] [--funcs f1,f2]
#                                            [--rec <recording dir>] [--calls N] [--jobs N] [--finite]
#   --export  run the Ghidra export first (only after a change of params.csv)
#   --funcs   gate with every recorded call, for these functions only (15 to 30 seconds)
#   --synth   synthetic calls (synth_pure.py) for these functions
#   --calls   calls for each synthetic function (default 90, use 30 to look around)
#   --jobs    synthetic functions that run at the same time (default 2)
#   --finite  no NaN, infinity or huge values (SYNTH_FINITE=1)
# The full client gate is not part of a step. Run it before a push or a merge:
#   python -I instrumentation/lift/gate.py --target client_win --jobs 2
# The logs are in build/step_client/. Open one only when the summary shows a failure.
set -uo pipefail
cd "$(dirname "$0")/../.."

export_flag=""
funcs=""
synth=""
rec="build/recordings/client-play2-20261103-2019"
calls=90
jobs=2
finite=""
while [ $# -gt 0 ]; do
    case "$1" in
        --export) export_flag="--export" ;;
        --funcs) funcs="$2"; shift ;;
        --synth) synth="$2"; shift ;;
        --rec) rec="$2"; shift ;;
        --calls) calls="$2"; shift ;;
        --jobs) jobs="$2"; shift ;;
        --finite) finite="1" ;;
        *) echo "unknown option $1"; exit 2 ;;
    esac
    shift
done

logs=build/step_client
mkdir -p "$logs"

echo "== build"
bash instrumentation/client/pipeline.sh $export_flag --no-gate > "$logs/pipeline.log" 2>&1
if grep -q -i -E "error C|erro C|build failed|error LNK|fatal error|FAILED" "$logs/pipeline.log"; then
    echo "build: FAILED (log $logs/pipeline.log)"
    grep -i -E "error C|erro C|error LNK|fatal error" "$logs/pipeline.log" | head -5
    exit 1
fi
echo "build: ok"
status=0

if [ -n "$funcs" ]; then
    echo "== gate with every recorded call: $funcs"
    python -I instrumentation/lift/gate.py --target client_win --all-calls --funcs "$funcs" --jobs "$jobs" > "$logs/gate_funcs.log" 2>&1
    gate_status=$?
    tail -3 "$logs/gate_funcs.log"
    # The gate prints only the calls that are the same. The lines of each function show the differences.
    pattern="$(echo "$funcs" | sed 's/,/|/g')"
    for result in build/gate/client_win/reset/all/client-*.txt; do
        echo "-- $(basename "$result" .txt)"
        grep -E "^\[ *[0-9]+/[0-9]+\] ($pattern) " "$result" | cut -c1-180
        if grep -E "^\[ *[0-9]+/[0-9]+\] ($pattern) " "$result" | grep -Eq '[1-9][0-9]* (different|crashed)'; then
            status=1
        fi
    done
    if [ "$gate_status" != 0 ]; then
        status=1
    fi
fi

if [ -n "$synth" ]; then
    echo "== synthetic calls ($calls each, $jobs at a time): $synth"
    mkdir -p "$logs/synth"
    SYNTH_CALLS="$calls" SYNTH_JOBS="$jobs" SYNTH_FINITE="$finite" PATH="build/client_deps:$PATH" \
        python -I instrumentation/client/synth_pure.py "$rec" "$logs/synth" "$synth" > "$logs/synth.log" 2>&1
    synth_status=$?
    grep -E "calls same|Traceback|Error" "$logs/synth.log"
    if [ "$synth_status" != 0 ]; then
        status=1
    fi
fi
echo "== done"
exit "$status"
