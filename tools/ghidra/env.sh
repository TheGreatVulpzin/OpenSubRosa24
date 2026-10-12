# Variáveis comuns dos scripts do Ghidra (Windows/Git Bash e Linux).
# Uso: source "$(dirname "$0")/env.sh"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        GHIDRA="${GHIDRA:-/e/tools/ghidra_12.1.4_PUBLIC}"
        export JAVA_HOME="${JAVA_HOME:-/c/Program Files/Eclipse Adoptium/jdk-21.0.7.6-hotspot}"
        HEADLESS="$GHIDRA/support/analyzeHeadless.bat" ;;
    *)
        GHIDRA="${GHIDRA:-$HOME/tools/ghidra_12.1.4_PUBLIC}"
        HEADLESS="$GHIDRA/support/analyzeHeadless" ;;
esac
PROJECT="$ROOT/ghidra-project"
