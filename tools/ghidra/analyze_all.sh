#!/usr/bin/env bash
# Importa os binários no projeto Ghidra local (ghidra-project/) e exporta as features.
# Uso: GHIDRA=/e/tools/ghidra_12.1.4_PUBLIC tools/ghidra/analyze_all.sh
set -euo pipefail
# Com argumentos, roda só os nomes curtos pedidos. Ex.: analyze_all.sh ded024c_linux ded024c_win
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GHIDRA="${GHIDRA:-/e/tools/ghidra_12.1.4_PUBLIC}"
HEADLESS="$GHIDRA/support/analyzeHeadless.bat"
mkdir -p "$ROOT/ghidra-project" "$ROOT/build/features"

ONLY=" $* "
run() {  # $1 = arquivo em original/, $2 = nome curto
    [ "$ONLY" = "  " ] || [[ "$ONLY" == *" $2 "* ]] || return 0
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
run "vanilla/0.24c_Dedicated_Linux/Sub Rosa 0.24c Dedicated Linux/subrosadedicated024c" ded024c_linux
run "vanilla/0.24c_Dedicated_Windows/Sub Rosa 0.24c Dedicated Windows/subrosadedicated024c.exe" ded024c_win
run "vanilla/0.24b_Dedicated_Linux/Sub Rosa 0.24b Dedicated Linux/subrosadedicated024b" ded024b_linux
