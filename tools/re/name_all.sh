#!/usr/bin/env bash
# Gera os CSVs de nomes de função dos binários sem símbolos.
# Requer build/features/*.jsonl (tools/ghidra/export_all.sh).
set -euo pipefail
cd "$(dirname "$0")/../.."
export PYTHONIOENCODING=utf-8
M="python -I tools/re/match_functions.py"
F=build/features
mkdir -p build/match analysis/client_win analysis/ded0802 analysis/024d

# 1. Cliente Windows a partir dos binários com símbolos.
$M --target $F/client_win.jsonl --ref $F/linux_x64.jsonl --ref $F/mac_i386.jsonl \
   --out analysis/client_win/functions.csv

# 2. Dedicado 02/08: mesmo build e compilador do cliente (modo estrito) + direto.
$M --target $F/ded0802.jsonl --ref "$F/client_win.jsonl:analysis/client_win/functions.csv" --strict \
   --out build/match/ded0802_via_client.csv
$M --target $F/ded0802.jsonl --ref $F/linux_x64.jsonl --ref $F/mac_i386.jsonl \
   --out build/match/ded0802_direct.csv
python -I tools/re/merge_names.py --out analysis/ded0802/functions.csv \
   build/match/ded0802_via_client.csv build/match/ded0802_direct.csv

# 3. Dedicado 024d: todas as referências juntas + direto.
$M --target $F/ded024d.jsonl \
   --ref "$F/ded0802.jsonl:analysis/ded0802/functions.csv" \
   --ref "$F/client_win.jsonl:analysis/client_win/functions.csv" \
   --ref $F/linux_x64.jsonl --ref $F/mac_i386.jsonl \
   --out build/match/ded024d_all.csv
$M --target $F/ded024d.jsonl --ref $F/linux_x64.jsonl --ref $F/mac_i386.jsonl \
   --out build/match/ded024d_direct.csv
python -I tools/re/merge_names.py --out analysis/024d/functions.csv --manual analysis/024d/manual_names.csv \
   build/match/ded024d_all.csv build/match/ded024d_direct.csv
