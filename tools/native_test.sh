#!/usr/bin/env bash
# Build the native client and run the smoke test on it. Run it in Git Bash from any folder.
#
#   bash tools/native_test.sh [--guard] [--hidden] [--build-only] [--timeout N] [scenario ...]
#
#   --guard       use the executable with a guard after each datum (a write past the end of an object
#                 prints "GUARD <name>" and the function that wrote it)
#   --hidden      hide the game window: the run takes no screen, no focus and no mouse
#   --build-only  build and check the executable, run no scenario
#   scenarios     menus, level, play, ... (default: menus level)
#
# The captures go to /c/svnt/<plain|guard> (SV_NT_OUT changes the folder).
#
# The script stops with exit code 1 when:
#   - the build prints an error or returns a code other than 0
#   - an executable is older than the start of the script (an old executable must not be tested)
#   - a scenario fails, or a guard reports a write past the end of an object
set -u

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root" || exit 1

guard=0
hidden=0
build_only=0
timeout=300
scenarios=()
while [ $# -gt 0 ]; do
    case "$1" in
        --guard) guard=1 ;;
        --hidden) hidden=1 ;;
        --build-only) build_only=1 ;;
        --timeout) shift; timeout="$1" ;;
        *) scenarios+=("$1") ;;
    esac
    shift
done
[ ${#scenarios[@]} -eq 0 ] && scenarios=(menus level)

if [ $guard -eq 1 ]; then
    kind=guard
    targets="subvulpes-client-native-guard-smoke subvulpes-client-native-guard"
    exes="opensubrosa24_client_native_guard-smoke opensubrosa24_client_native_guard"
    smoke=opensubrosa24_client_native_guard-smoke
else
    kind=plain
    targets="subvulpes-client-native-smoke subvulpes-client-native"
    exes="opensubrosa24_client_native_smoke opensubrosa24_client_native"
    smoke=opensubrosa24_client_native_smoke
fi

fail() { echo "FAIL: $*"; exit 1; }

started=$(date +%s)
echo "== build ($kind): $targets"
if ! cmd //c "tools\\build_native.bat $targets"; then
    fail "build (see build/native_build.log)"
fi

# An executable must be newer than every source file. A build that failed or was skipped leaves an
# old file, and a test on it would test old code.
for name in $exes; do
    [ -f "build/hook/$name.exe" ] || fail "build/hook/$name.exe does not exist"
done
# Only the executable that the test runs has to hold every source (the other one may skip the smoke driver).
file="build/hook/$smoke.exe"
newer=$(find src third_party CMakeLists.txt -type f \
    \( -name '*.c' -o -name '*.h' -o -name '*.inc' -o -name 'CMakeLists.txt' \) -newer "$file" | head -n 1)
[ -z "$newer" ] || fail "$file is older than $newer: the build did not make it"
echo "== executables are current"
[ $build_only -eq 1 ] && exit 0

export SR_SMOKE_HIDDEN=$hidden
# The capture path must stay short (the driver limits it), so the output is outside the repository.
out_root="${SV_NT_OUT:-/c/svnt}"
output="$out_root/$kind"
mkdir -p "$out_root"
rm -rf "$output"
echo "== smoke: ${scenarios[*]} (hidden=$hidden)"
python -I instrumentation/client/smoke.py "build/hook/$smoke.exe" "${scenarios[@]}" --timeout "$timeout" \
    --output "$output" > "$output.log" 2>&1
grep -E "^\S+/[a-z]+: " "$output.log" || true
bad=0
grep -qE "/[a-z]+: FAILED" "$output.log" && bad=1

if [ $guard -eq 1 ]; then
    echo "== guards"
    # Each GUARD line has a SAMPLE line after it. Map its addresses to function names.
    python -I - "$output" "build/hook/$smoke.map" <<'EOF'
import glob, subprocess, sys

output, map_file = sys.argv[1], sys.argv[2]
seen = {}
for log in sorted(glob.glob(output + "/*/*/run.log")):
    lines = open(log, errors="replace").read().replace("CRASH", "\nCRASH").splitlines()
    for i, line in enumerate(lines):
        if line.startswith("GUARD ") and i + 1 < len(lines) and lines[i + 1].startswith("SAMPLE"):
            name = line.split()[1]
            if name in seen:
                continue
            open(output + ".sample", "w").write(lines[i + 1] + "\n")
            where = subprocess.run([sys.executable, "-I", "tools/re/map_address.py", map_file, output + ".sample"],
                                   capture_output=True, text=True).stdout.strip()
            seen[name] = (line, where.split(" <- ")[0])
for line, where in seen.values():
    print(" ", line, "->", where)
print("guards that fired:", len(seen))
sys.exit(1 if seen else 0)
EOF
    [ $? -eq 0 ] || bad=1
fi

[ $bad -eq 0 ] || fail "see $output.log"
echo "== all good"
