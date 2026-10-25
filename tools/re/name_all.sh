#!/usr/bin/env bash
# Generate the function-name CSVs for the binaries without symbols.
# Needs build/features/*.jsonl (instrumentation/ghidra/export_all.sh).
set -euo pipefail
cd "$(dirname "$0")/../.."
export PYTHONIOENCODING=utf-8
M="python -I tools/re/match_functions.py"
F=build/features
mkdir -p build/match analysis/client_win analysis/ded0802 analysis/024d

# 1. Windows client, from the binaries with symbols.
$M --target $F/client_win.jsonl --ref $F/linux_x64.jsonl --ref $F/mac_i386.jsonl \
   --out analysis/client_win/functions.csv

# 2. 08/02 dedicated server: same build and compiler as the client (strict mode) + direct.
$M --target $F/ded0802.jsonl --ref "$F/client_win.jsonl:analysis/client_win/functions.csv" --strict \
   --out build/match/ded0802_via_client.csv
$M --target $F/ded0802.jsonl --ref $F/linux_x64.jsonl --ref $F/mac_i386.jsonl \
   --out build/match/ded0802_direct.csv
python -I tools/re/merge_names.py --out analysis/ded0802/functions.csv \
   build/match/ded0802_via_client.csv build/match/ded0802_direct.csv

# 3. 024d dedicated server. Cycle: match functions -> map globals -> match again.
#    References: Linux 0.24c/0.24b servers (same version, with names), clients
#    with names (0.24b and Mac 0.25, the closest to the 024d after it) and the MSVC .exe files already named.
cp analysis/024d/functions.csv build/match/ded024d_prev.csv 2>/dev/null || true
for i in 1 2 3; do
    python -I tools/re/map_globals.py --target $F/ded024d.jsonl --names build/match/ded024d_prev.csv        --ref $F/ded024c_linux.jsonl --ref $F/ded024b_linux.jsonl --ref $F/mac_i386.jsonl        --out analysis/024d/globals.csv
    $M --target $F/ded024d.jsonl --target-globals analysis/024d/globals.csv        --ref $F/ded024c_linux.jsonl --ref $F/ded024b_linux.jsonl        --ref $F/linux_x64.jsonl --ref $F/mac_i386.jsonl --ref $F/mac025_i386.jsonl        --ref "$F/ded0802.jsonl:analysis/ded0802/functions.csv"        --ref "$F/client_win.jsonl:analysis/client_win/functions.csv"        --seed analysis/024d/manual_names.csv        --out build/match/ded024d_iter$i.csv
    python -I tools/re/merge_names.py --out build/match/ded024d_prev.csv --manual analysis/024d/manual_names.csv        build/match/ded024d_iter$i.csv build/match/ded024d_prev.csv
done
cp build/match/ded024d_prev.csv analysis/024d/functions.csv
