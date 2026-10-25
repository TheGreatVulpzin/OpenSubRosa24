#!/usr/bin/env bash
# Re-export the features of the programs already analyzed in the project (without re-analyzing).
# Usage: instrumentation/ghidra/export_all.sh
set -euo pipefail
source "$(dirname "$0")/env.sh"

export_one() {  # $1 = program name in the project, $2 = short name, $3 = folder in the project (optional)
    "$HEADLESS" "$ROOT/ghidra-project" "subvulpes${3:+/$3}" \
        -process "$1" -noanalysis -readOnly \
        -scriptPath "$ROOT/instrumentation/ghidra" \
        -postScript ExportFunctionFeatures.java "$ROOT/build/features/$2.jsonl" 2>&1 \
        | grep -E "Exported|ERROR" || true
}

export_one subrosadedicated024d.exe ded024d
export_one subrosadedicated.exe     ded0802
export_one subrosa.x64              linux_x64
export_one subrosa                  mac_i386
export_one subrosa.exe              client_win
export_one subrosadedicated024c     ded024c_linux
export_one subrosadedicated024c.exe ded024c_win
export_one subrosadedicated024b     ded024b_linux
export_one subrosa                  mac025_i386   mac025
