#!/usr/bin/env bash
# Prepare a Linux cloud session for RE work:
#   1. Java 21 (if missing)
#   2. Ghidra 12.1.4 in ~/tools (with SHA-256 check)
#   3. Official Sub Rosa binaries in original/ (downloaded from gart.sh)
#
# The Black Houser 024d is identical to the official 024d, and the group's client
# is the official 0.24b. For this reason the paths original/client and original/blackhouser
# can be assembled from the official packages.
#
# Usage: bash tools/cloud/setup.sh
# Then: bash instrumentation/ghidra/analyze_all.sh   (10 to 30 minutes)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TOOLS="${TOOLS:-$HOME/tools}"
GHIDRA_VER=12.1.4
GHIDRA_ZIP=ghidra_${GHIDRA_VER}_PUBLIC_20260921.zip
GHIDRA_SHA=ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db
GART="https://crypticsea.gart.sh/Sub%20Rosa"

# ---------------------------------------------------------------- Java
if ! java -version 2>&1 | grep -qE 'version "(2[1-9]|[3-9][0-9])'; then
    echo "== Installing Java 21"
    SUDO=""
    if [ "$(id -u)" != 0 ] && command -v sudo >/dev/null; then SUDO=sudo; fi
    if command -v apt-get >/dev/null; then
        $SUDO apt-get update -qq && $SUDO apt-get install -y -qq openjdk-21-jdk-headless unzip
    else
        echo "ERROR: no apt-get. Install a JDK 21 by hand." >&2
        exit 1
    fi
fi

if ! command -v unzip >/dev/null; then
    echo "== Installing unzip"
    $( [ "$(id -u)" != 0 ] && command -v sudo ) apt-get install -y -qq unzip
fi

# ---------------------------------------------------------------- Ghidra
mkdir -p "$TOOLS"
if [ ! -x "$TOOLS/ghidra_${GHIDRA_VER}_PUBLIC/support/analyzeHeadless" ]; then
    echo "== Downloading Ghidra $GHIDRA_VER"
    curl -sfL -o "$TOOLS/$GHIDRA_ZIP" \
        "https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_${GHIDRA_VER}_build/$GHIDRA_ZIP"
    echo "$GHIDRA_SHA  $TOOLS/$GHIDRA_ZIP" | sha256sum -c -
    (cd "$TOOLS" && unzip -q "$GHIDRA_ZIP" && rm "$GHIDRA_ZIP")
fi
echo "GHIDRA=$TOOLS/ghidra_${GHIDRA_VER}_PUBLIC"

# ---------------------------------------------------------------- binaries
fetch() {  # $1 = relative path on gart.sh (with %20), $2 = destination folder in original/vanilla
    local dest="$ROOT/original/vanilla/$2" arch="$ROOT/original/vanilla/archives/$(basename "${1//%20/ }")"
    [ -d "$dest" ] && return 0
    mkdir -p "$ROOT/original/vanilla/archives"
    # Download to .part and create the folder only afterwards: a network failure leaves no empty folder
    # (which would make the next run skip the download).
    if [ ! -f "$arch" ]; then
        curl -sfL -o "$arch.part" "$GART/$1" || { rm -f "$arch.part"; echo "ERROR: failed to download $1" >&2; return 1; }
        mv "$arch.part" "$arch"
    fi
    mkdir -p "$dest"
    case "$arch" in
        *.zip) unzip -q -o "$arch" -d "$dest" ;;
        *.tar) tar -xf "$arch" -C "$dest" ;;
    esac || { rm -rf "$dest"; return 1; }
    echo "ok $2"
}
echo "== Downloading official binaries"
fetch "0.24/b/Sub%20Rosa%200.24b.zip"                          0.24b
fetch "0.24/b/Sub%20Rosa%200.24b%20Dedicated%20Linux.zip"      0.24b_Dedicated_Linux
fetch "0.24/b/Sub%20Rosa%200.24b%20Dedicated%20Windows.zip"    0.24b_Dedicated_Windows
fetch "0.24/c/Sub%20Rosa%200.24c%20Dedicated%20Linux.zip"      0.24c_Dedicated_Linux
fetch "0.24/c/Sub%20Rosa%200.24c%20Dedicated%20Windows.zip"    0.24c_Dedicated_Windows
fetch "0.24/Sub%20Rosa%200.24d%20Dedicated%20Windows.zip"      0.24d_Dedicated_Windows
fetch "0.25/b/Sub%20Rosa%200.25b%20Dedicated%20Linux.tar"      0.25b_Dedicated_Linux
fetch "0.25/b/Sub%20Rosa%200.25b.zip"                          0.25b   # Mac i386 client with symbols

# Assemble original/client (0.24b client) and original/blackhouser (024d) as on the PC.
V="$ROOT/original/vanilla"
if [ ! -e "$ROOT/original/client/subrosa.x64" ]; then
    mkdir -p "$ROOT/original/client"
    cp -r "$V/0.24b/Sub Rosa 0.24b/." "$ROOT/original/client/"
fi
if [ ! -e "$ROOT/original/blackhouser/subrosadedicated024d.exe" ]; then
    mkdir -p "$ROOT/original/blackhouser"
    cp -r "$V/0.24d_Dedicated_Windows/Sub Rosa 0.24d Dedicated Windows/." "$ROOT/original/blackhouser/"
fi

# Check the hashes of the main executables against the PC manifest.
echo "== Checking hashes"
cd "$ROOT/original"
grep -E '  (client/subrosa\.(exe|x64)|client/subrosadedicated\.exe|blackhouser/subrosadedicated024d\.exe|client/subrosa\.app/Contents/MacOS/subrosa)$' MANIFEST.sha256 \
    | sha256sum -c -
echo "== Done"
