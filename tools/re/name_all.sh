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
   --out build/match/client_win_matched.csv
#    Same compiler as the 024d (strict). The names of the 024d come back here for functions the
#    first pass left open. Low votes give low confidence: check them before a rewrite.
$M --target $F/client_win.jsonl --ref "$F/ded024d.jsonl:analysis/024d/functions.csv" --strict \
   --keep build/match/client_win_matched.csv --out build/match/client_win_via024d.csv
#    GLee stubs and link functions: names come from the machine code (tools/re/name_glee.py).
python -I tools/re/name_glee.py --exe original/client/subrosa.exe --target $F/client_win.jsonl \
   --ref $F/linux_x64.jsonl --out analysis/client_win/glee_names.csv \
   --globals analysis/client_win/manual_globals.csv
(head -1 analysis/client_win/glee_names.csv; tail -qn +2 analysis/client_win/glee_names.csv analysis/client_win/manual_names.csv) \
   > build/match/client_win_manual.csv
python -I tools/re/merge_names.py --out build/match/client_win_merged.csv \
   --manual build/match/client_win_manual.csv build/match/client_win_matched.csv build/match/client_win_via024d.csv
#    Open functions that hold the same strings as a reference function (size changed a lot).
python -I tools/re/match_strings.py --target $F/client_win.jsonl --ref $F/linux_x64.jsonl --ref $F/mac_i386.jsonl \
   --names build/match/client_win_merged.csv --out build/match/client_win_strings.csv
python -I tools/re/merge_names.py --out build/match/client_win_merged2.csv \
   build/match/client_win_merged.csv build/match/client_win_strings.csv
#    Open functions that call the same named functions as a reference function.
python -I tools/re/match_callees.py --target $F/client_win.jsonl --names build/match/client_win_merged2.csv \
   --ref $F/linux_x64.jsonl --ref "$F/ded024d.jsonl:analysis/024d/functions.csv" \
   --out build/match/client_win_callees.csv
python -I tools/re/merge_names.py --out analysis/client_win/functions.csv \
   build/match/client_win_merged2.csv build/match/client_win_callees.csv
#    Candidates only: check each global before it goes into manual_globals.csv.
python -I tools/re/map_globals.py --target $F/client_win.jsonl --names analysis/client_win/functions.csv \
   --ref $F/linux_x64.jsonl --ref $F/mac_i386.jsonl --out analysis/client_win/globals.csv

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
