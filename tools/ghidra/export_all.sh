#!/usr/bin/env bash
# Reexporta as features dos programas já analisados no projeto (sem reanalisar).
# Uso: tools/ghidra/export_all.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
GHIDRA="${GHIDRA:-/e/tools/ghidra_12.1.4_PUBLIC}"
export JAVA_HOME="${JAVA_HOME:-/c/Program Files/Eclipse Adoptium/jdk-21.0.7.6-hotspot}"

export_one() {  # $1 = nome do programa no projeto, $2 = nome curto
    "$GHIDRA/support/analyzeHeadless.bat" "$ROOT/ghidra-project" subvulpes \
        -process "$1" -noanalysis -readOnly \
        -scriptPath "$ROOT/tools/ghidra" \
        -postScript ExportFunctionFeatures.java "$ROOT/build/features/$2.jsonl" 2>&1 \
        | grep -E "Exportado|ERROR" || true
}

export_one subrosadedicated024d.exe ded024d
export_one subrosadedicated.exe     ded0802
export_one subrosa.x64              linux_x64
export_one subrosa                  mac_i386
export_one subrosa.exe              client_win
export_one subrosadedicated024c     ded024c_linux
export_one subrosadedicated024c.exe ded024c_win
export_one subrosadedicated024b     ded024b_linux
