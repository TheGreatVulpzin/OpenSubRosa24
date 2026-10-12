#!/usr/bin/env bash
# Prepara uma sessão Linux na nuvem para o trabalho de RE:
#   1. Java 21 (se faltar)
#   2. Ghidra 12.1.4 em ~/tools (com verificação de SHA-256)
#   3. Binários oficiais do Sub Rosa em original/ (baixados do gart.sh)
#
# O 024d do Black Houser é idêntico ao 024d oficial, e o cliente da galera
# é o 0.24b oficial. Por isso os caminhos original/client e original/blackhouser
# podem ser montados a partir dos pacotes oficiais.
#
# Uso: bash tools/cloud/setup.sh
# Depois: bash tools/ghidra/analyze_all.sh   (10 a 30 minutos)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TOOLS="${TOOLS:-$HOME/tools}"
GHIDRA_VER=12.1.4
GHIDRA_ZIP=ghidra_${GHIDRA_VER}_PUBLIC_20260921.zip
GHIDRA_SHA=ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db
GART="https://crypticsea.gart.sh/Sub%20Rosa"

# ---------------------------------------------------------------- Java
if ! java -version 2>&1 | grep -qE 'version "(2[1-9]|[3-9][0-9])'; then
    echo "== Instalando Java 21"
    SUDO=""
    if [ "$(id -u)" != 0 ] && command -v sudo >/dev/null; then SUDO=sudo; fi
    if command -v apt-get >/dev/null; then
        $SUDO apt-get update -qq && $SUDO apt-get install -y -qq openjdk-21-jdk-headless unzip
    else
        echo "ERRO: sem apt-get. Instale um JDK 21 manualmente." >&2
        exit 1
    fi
fi

if ! command -v unzip >/dev/null; then
    echo "== Instalando unzip"
    $( [ "$(id -u)" != 0 ] && command -v sudo ) apt-get install -y -qq unzip
fi

# ---------------------------------------------------------------- Ghidra
mkdir -p "$TOOLS"
if [ ! -x "$TOOLS/ghidra_${GHIDRA_VER}_PUBLIC/support/analyzeHeadless" ]; then
    echo "== Baixando Ghidra $GHIDRA_VER"
    curl -sfL -o "$TOOLS/$GHIDRA_ZIP" \
        "https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_${GHIDRA_VER}_build/$GHIDRA_ZIP"
    echo "$GHIDRA_SHA  $TOOLS/$GHIDRA_ZIP" | sha256sum -c -
    (cd "$TOOLS" && unzip -q "$GHIDRA_ZIP" && rm "$GHIDRA_ZIP")
fi
echo "GHIDRA=$TOOLS/ghidra_${GHIDRA_VER}_PUBLIC"

# ---------------------------------------------------------------- binários
fetch() {  # $1 = caminho relativo no gart.sh (com %20), $2 = pasta de destino em original/vanilla
    local dest="$ROOT/original/vanilla/$2" arch="$ROOT/original/vanilla/archives/$(basename "${1//%20/ }")"
    [ -d "$dest" ] && return 0
    mkdir -p "$ROOT/original/vanilla/archives"
    # Baixa para .part e só cria a pasta depois: falha de rede não deixa pasta vazia
    # (que faria a próxima execução pular o download).
    if [ ! -f "$arch" ]; then
        curl -sfL -o "$arch.part" "$GART/$1" || { rm -f "$arch.part"; echo "ERRO: falha ao baixar $1" >&2; return 1; }
        mv "$arch.part" "$arch"
    fi
    mkdir -p "$dest"
    case "$arch" in
        *.zip) unzip -q -o "$arch" -d "$dest" ;;
        *.tar) tar -xf "$arch" -C "$dest" ;;
    esac || { rm -rf "$dest"; return 1; }
    echo "ok $2"
}
echo "== Baixando binários oficiais"
fetch "0.24/b/Sub%20Rosa%200.24b.zip"                          0.24b
fetch "0.24/b/Sub%20Rosa%200.24b%20Dedicated%20Linux.zip"      0.24b_Dedicated_Linux
fetch "0.24/b/Sub%20Rosa%200.24b%20Dedicated%20Windows.zip"    0.24b_Dedicated_Windows
fetch "0.24/c/Sub%20Rosa%200.24c%20Dedicated%20Linux.zip"      0.24c_Dedicated_Linux
fetch "0.24/c/Sub%20Rosa%200.24c%20Dedicated%20Windows.zip"    0.24c_Dedicated_Windows
fetch "0.24/Sub%20Rosa%200.24d%20Dedicated%20Windows.zip"      0.24d_Dedicated_Windows
fetch "0.25/b/Sub%20Rosa%200.25b%20Dedicated%20Linux.tar"      0.25b_Dedicated_Linux
fetch "0.25/b/Sub%20Rosa%200.25b.zip"                          0.25b   # cliente Mac i386 com símbolos

# Monta original/client (cliente 0.24b) e original/blackhouser (024d) como no PC.
V="$ROOT/original/vanilla"
if [ ! -e "$ROOT/original/client/subrosa.x64" ]; then
    mkdir -p "$ROOT/original/client"
    cp -r "$V/0.24b/Sub Rosa 0.24b/." "$ROOT/original/client/"
fi
if [ ! -e "$ROOT/original/blackhouser/subrosadedicated024d.exe" ]; then
    mkdir -p "$ROOT/original/blackhouser"
    cp -r "$V/0.24d_Dedicated_Windows/Sub Rosa 0.24d Dedicated Windows/." "$ROOT/original/blackhouser/"
fi

# Confere os hashes dos executáveis principais com o manifesto do PC.
echo "== Conferindo hashes"
cd "$ROOT/original"
grep -E '  (client/subrosa\.(exe|x64)|client/subrosadedicated\.exe|blackhouser/subrosadedicated024d\.exe|client/subrosa\.app/Contents/MacOS/subrosa)$' MANIFEST.sha256 \
    | sha256sum -c -
echo "== Pronto"
