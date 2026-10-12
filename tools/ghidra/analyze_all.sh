#!/usr/bin/env bash
# Importa os binários no projeto Ghidra local (ghidra-project/) e exporta as features.
# Uso: tools/ghidra/analyze_all.sh [nome_curto ...]   (caminhos em tools/ghidra/env.sh)
set -euo pipefail
source "$(dirname "$0")/env.sh"
# Com argumentos, roda só os nomes curtos pedidos. Ex.: analyze_all.sh ded024c_linux ded024c_win
mkdir -p "$ROOT/ghidra-project" "$ROOT/build/features"

ONLY=" $* "
run() {  # $1 = arquivo em original/, $2 = nome curto, $3 = pasta no projeto (opcional)
    [ "$ONLY" = "  " ] || [[ "$ONLY" == *" $2 "* ]] || return 0
    "$HEADLESS" "$ROOT/ghidra-project" "subvulpes${3:+/$3}" \
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
# Cliente Mac 0.25/0.25b (out/2015, mesmo binário): o binário com símbolos mais perto do 024d depois dele.
# Fica na pasta /mac025 porque o nome "subrosa" já é do Mac 0.24b.
run "vanilla/0.25b/Sub Rosa 0.25b/subrosa.app/Contents/MacOS/subrosa" mac025_i386 mac025
