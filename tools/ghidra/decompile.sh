#!/usr/bin/env bash
# Decompila funções de um programa já importado no projeto.
# Uso: tools/ghidra/decompile.sh <programa> <saida.c> [rva ...]
#   Sem RVAs: decompila todas as funções.
#   Ex.: tools/ghidra/decompile.sh subrosadedicated024c build/decomp/024c.c 0x37733
#   Programa em pasta do projeto: <pasta>/<programa> (ex.: mac025/subrosa).
set -euo pipefail
source "$(dirname "$0")/env.sh"
prog="$1"; out="$2"; shift 2
folder=""
case "$prog" in */*) folder="/${prog%/*}"; prog="${prog##*/}" ;; esac
mkdir -p "$(dirname "$out")"
"$HEADLESS" "$PROJECT" "subvulpes$folder" -process "$prog" -noanalysis -readOnly \
    -scriptPath "$ROOT/tools/ghidra" -postScript DecompileFunctions.java "$(cd "$(dirname "$out")" && pwd)/$(basename "$out")" "$@" 2>&1 \
    | grep -E "Decompilado|ERROR|Sem função" || true
