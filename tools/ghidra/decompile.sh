#!/usr/bin/env bash
# Decompile functions of a program already imported into the project.
# Usage: tools/ghidra/decompile.sh <program> <output.c> [rva ...]
#   Without RVAs: decompile all functions.
#   E.g.: tools/ghidra/decompile.sh subrosadedicated024c build/decomp/024c.c 0x37733
#   Program in a project folder: <folder>/<program> (e.g. mac025/subrosa).
set -euo pipefail
source "$(dirname "$0")/env.sh"
prog="$1"; out="$2"; shift 2
folder=""
case "$prog" in */*) folder="/${prog%/*}"; prog="${prog##*/}" ;; esac
mkdir -p "$(dirname "$out")"
"$HEADLESS" "$PROJECT" "subvulpes$folder" -process "$prog" -noanalysis -readOnly \
    -scriptPath "$ROOT/tools/ghidra" -postScript DecompileFunctions.java "$(cd "$(dirname "$out")" && pwd)/$(basename "$out")" "$@" 2>&1 \
    | grep -E "Decompiled|ERROR|No function" || true
