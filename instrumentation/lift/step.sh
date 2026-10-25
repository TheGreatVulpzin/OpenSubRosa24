#!/usr/bin/env bash
# One check of a rewrite step, with a short summary: lift and build (pipeline.sh), the gate on
# every recorded call of the functions, and the synthetic tests. The full logs go to build/step/.
#
#   bash instrumentation/lift/step.sh [--no-pipeline] [--rec <recording dir>] [--synth <name>,...] [--invalid] <function>,...
#
# --synth takes the names of instrumentation/lift/synth_<name>.py (items, vehicles, net, sys, types, area,
# level, buildings, humans, bonds) and needs --rec. The exit status is 1 when a check fails.
# --invalid also runs the security expectations. They fail before the server checks exist.
set -u
cd "$(dirname "$0")/../.."

pipeline=1
rec=""
synth=""
invalid=0
while [ $# -gt 0 ]; do
    case "$1" in
    --no-pipeline) pipeline=0; shift ;;
    --rec) rec="$2"; shift 2 ;;
    --synth) synth="$2"; shift 2 ;;
    --invalid) invalid=1; shift ;;
    -*) echo "unknown option: $1"; exit 2 ;;
    *) break ;;
    esac
done
funcs="${1:-}"
if [ -z "$funcs" ]; then
    echo "usage: bash instrumentation/lift/step.sh [--no-pipeline] [--rec <dir>] [--synth <name>,...] [--invalid] <function>,..."
    exit 2
fi
if [ $invalid -eq 1 ] && [ -z "$rec" ]; then
    echo "--invalid needs --rec <recording dir>"
    exit 2
fi
echo "functions: $funcs"
if [ -n "$synth" ] && [ -z "$rec" ]; then
    echo "--synth needs --rec <recording dir>"
    exit 2
fi

out=build/step
mkdir -p "$out"
fail=0

# A harness result line fails when it has a "different" or a "crashed" count above 0.
bad_lines() {
    grep -E "functions tested" "$1" | grep -E ", [1-9][0-9]* different|[1-9][0-9]* lifted crashed"
}

if [ $pipeline -eq 1 ]; then
    bash instrumentation/lift/pipeline.sh > "$out/pipeline.log" 2>&1
    errors=$(grep -E "error C|build failed" "$out/pipeline.log" | head -10)
    if [ -n "$errors" ]; then
        echo "build: FAILED (log $out/pipeline.log)"
        echo "$errors"
        exit 1
    fi
    echo "build: ok"
    drops=$(grep -E "^DROP" "$out/pipeline.log" | head -10)
    if [ -n "$drops" ]; then
        echo "quick gate: DROP"
        echo "$drops"
        fail=1
    elif grep -q "no count dropped" "$out/pipeline.log"; then
        echo "quick gate: ok"
    else
        echo "quick gate: no result (log $out/pipeline.log)"
        fail=1
    fi
fi

python -I instrumentation/lift/gate.py --all-calls --funcs "$funcs" > "$out/gate_all.log" 2>&1
drops=$(grep -E "^DROP" "$out/gate_all.log" | head -10)
if [ -n "$drops" ]; then
    echo "all-calls gate: DROP"
    echo "$drops"
    fail=1
elif grep -q "no count dropped" "$out/gate_all.log"; then
    echo "all-calls gate: ok ($(grep -c 'same (baseline' "$out/gate_all.log") recordings)"
else
    echo "all-calls gate: no result (log $out/gate_all.log)"
    fail=1
fi

IFS=',' read -r -a names <<< "$synth"
for name in "${names[@]}"; do
    [ -z "$name" ] && continue
    log="$out/synth_$name.log"
    work="$out/synth_$name"
    synth_status=0
    case "$name" in
    humans|bonds)
        rm -rf "$work"
        python -I "instrumentation/lift/synth_$name.py" "$rec" "$work" > "$log" 2>&1 || synth_status=$?
        target=humansimulation
        [ "$name" = bonds ] && target=bondsimulation2
        if [ "$name" = bonds ]; then
            cases="$work"
        else
            cases=$(ls -d "$work"/*/ 2>/dev/null)
        fi
        for c in $cases; do
            echo "$(basename "$c") $(build/hook/harness.exe build/hook/024d_fixed.exe "$c/sv_snapshot.bin" "$c/sv_args.txt" $target 2>&1 | grep 'functions tested')" >> "$log"
        done
        rm -rf "$work"
        ;;
    sys|level|buildings|area|logic)
        python -I "instrumentation/lift/synth_$name.py" --run "$rec" "$work" > "$log" 2>&1 || synth_status=$?
        ;;
    *)
        python -I "instrumentation/lift/synth_$name.py" --run --no-cover "$rec" "$work" > "$log" 2>&1 || synth_status=$?
        ;;
    esac
    total=$(grep -c "functions tested" "$log")
    bad=$(bad_lines "$log")
    if [ "$synth_status" -ne 0 ] || grep -q 'NO RESULT' "$log"; then
        echo "synth $name: FAILED (exit $synth_status, log $log)"
        fail=1
    elif [ "$total" -eq 0 ]; then
        echo "synth $name: no result (log $log)"
        fail=1
    elif [ -n "$bad" ]; then
        echo "synth $name: $(echo "$bad" | wc -l) of $total cases differ"
        echo "$bad" | head -5
        fail=1
    else
        echo "synth $name: ok ($total cases)"
    fi
done

if [ $invalid -eq 1 ]; then
    log="$out/security.log"
    python -I instrumentation/lift/synth_net.py --invalid --run "$rec" "$out/security" > "$log" 2>&1
    status=$?
    grep -E '^security:|^[a-z0-9_]+: FAIL' "$log"
    if [ $status -ne 0 ] || ! grep -q '^security: .* cases, 0 failed$' "$log"; then
        echo "invalid input: FAILED (log $log)"
        fail=1
    else
        echo "invalid input: ok"
    fi
fi

exit $fail
