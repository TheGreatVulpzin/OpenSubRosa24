#!/usr/bin/env bash
# Import the binaries into the local Ghidra project (ghidra-project/) and export the features.
# Usage: instrumentation/ghidra/analyze_all.sh [short_name ...]   (paths in instrumentation/ghidra/env.sh)
set -euo pipefail
source "$(dirname "$0")/env.sh"
# With arguments, run only the requested short names. E.g.: analyze_all.sh ded024c_linux ded024c_win
mkdir -p "$ROOT/ghidra-project" "$ROOT/build/features"

ONLY=" $* "
run() {  # $1 = file in original/, $2 = short name, $3 = folder in the project (optional)
    [ "$ONLY" = "  " ] || [[ "$ONLY" == *" $2 "* ]] || return 0
    "$HEADLESS" "$ROOT/ghidra-project" "subvulpes${3:+/$3}" \
        -import "$ROOT/original/$1" -overwrite \
        -scriptPath "$ROOT/instrumentation/ghidra" \
        -postScript ExportFunctionFeatures.java "$ROOT/build/features/$2.jsonl" \
        -log "$ROOT/build/features/$2.log"
}

run blackhouser/subrosadedicated024d.exe ded024d
run client/subrosadedicated.exe          ded0802
run client/subrosa.x64                   linux_x64
run client/subrosa.app/Contents/MacOS/subrosa mac_i386
run client/subrosa.exe                   client_win
run "vanilla/0.24c_Dedicated_Linux/Sub Rosa 0.24c Dedicated Linux/subrosadedicated024c" ded024c_linux
run "vanilla/0.24c_Dedicated_Windows/Sub Rosa 0.24c Dedicated Windows/subrosadedicated024c.exe" ded024c_win
run "vanilla/0.24b_Dedicated_Linux/Sub Rosa 0.24b Dedicated Linux/subrosadedicated024b" ded024b_linux
# Mac 0.25/0.25b client (Oct 2015, same binary): the binary with symbols closest to the 024d after it.
# It sits in the /mac025 folder because the name "subrosa" already belongs to the Mac 0.24b.
run "vanilla/0.25b/Sub Rosa 0.25b/subrosa.app/Contents/MacOS/subrosa" mac025_i386 mac025
