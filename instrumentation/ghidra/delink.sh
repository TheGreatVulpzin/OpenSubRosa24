#!/usr/bin/env bash
# Export a range of the 024d as a COFF object file with relocations (ghidra-delinker-extension).
# Usage: instrumentation/ghidra/delink.sh <first VA> <last VA> <output.obj>
#   E.g.: instrumentation/ghidra/delink.sh 0040ad30 0040ad6a build/delink/init_genrand.obj
# The script runs the "Relocation table synthesizer" analyzer on the range itself. The project
# opens read-only, so nothing is saved. Check the result with: dumpbin /symbols /relocations /disasm
set -euo pipefail
source "$(dirname "$0")/env.sh"
first="$1"; last="$2"; out="$3"
mkdir -p "$(dirname "$out")"
# Git Bash would turn the "/exporter" style arguments of DelinkProgram.java into Windows paths.
export MSYS2_ARG_CONV_EXCL='*'
EXT="${GHIDRA_DELINKER_SCRIPTS:-E:/tools/ghidra_12.1.4_PUBLIC/Ghidra/Extensions/ghidra-delinker-extension/ghidra_scripts}"
"$HEADLESS" "$(cygpath -m "$PROJECT")" subvulpes -process subrosadedicated024d.exe -noanalysis -readOnly \
    -scriptPath "$EXT" -postScript DelinkProgram.java /exporter "COFF relocatable object" \
    /include-range "$first-$last" /export "$(cygpath -m "$(cd "$(dirname "$out")" && pwd)")/$(basename "$out")" 2>&1 \
    | grep -E "ERROR|Exception|Failed" || true
ls -l "$out"
