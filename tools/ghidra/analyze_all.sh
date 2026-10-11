#!/usr/bin/env bash
# Importa os binários no projeto Ghidra local (ghidra-project/) e exporta as features.
# Uso: GHIDRA=/e/tools/ghidra_12.1.4_PUBLIC tools/ghidra/analyze_all.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GHIDRA="${GHIDRA:-/e/tools/ghidra_12.1.4_PUBLIC}"
HEADLESS="$GHIDRA/support/analyzeHeadless.bat"
mkdir -p "$ROOT/ghidra-project" "$ROOT/build/features"

run() {  # $1 = arquivo em original/, $2 = nome curto
    "$HEADLESS" "$ROOT/ghidra-project" subvulpes \
        -import "$ROOT/original/$1" -overwrite \
        -scriptPath "$ROOT/tools/ghidra" \
        -postScript ExportFunctionFeatures.java "$ROOT/build/features/$2.jsonl" \
        -log "$ROOT/build/features/$2.log"
}

run blackhouser/subrosadedicated024d.exe ded024d
run client/subrosadedicated.exe          ded0802
run client/subrosa.x64                   linux_x64
run client/subrosa.app/Contents/MacOS/subrosa mac_i386
run client/subrosa.exe                   client_win
