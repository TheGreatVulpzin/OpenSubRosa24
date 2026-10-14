#!/usr/bin/env bash
# Copy the Ghidra project for the GhidraMCP server (.mcp.json).
#
# The MCP server opens a copy, not ghidra-project/: a Ghidra project has one lock, and the
# headless scripts (LiftExport.java, ApplyTypes.java...) need the real project. The MCP
# server opens the copy with --discard, so nothing that it changes is saved. Changes that
# must stay go into a script or a CSV in the repository, then into the real project.
#
# Run it again after each change to the real project (ApplyTypes.java, FixReturnTypes.java).
# Usage: bash tools/ghidra/mcp_refresh.sh
set -euo pipefail
source "$(dirname "$0")/env.sh"
COPY="${GHIDRA_MCP_PROJECT:-/e/subvulpes/ghidra-mcp-project}"
rm -rf "$COPY"
mkdir -p "$COPY"
cp -r "$PROJECT"/subvulpes.gpr "$PROJECT"/subvulpes.rep "$COPY"/
rm -f "$COPY"/*.lock "$COPY"/*.lock~
echo "copied $PROJECT -> $COPY"
