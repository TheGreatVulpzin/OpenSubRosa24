#!/usr/bin/env bash
# Copy the Ghidra project again for the GhidraMCP server and start it again.
#
# The server runs as its own process with the HTTP transport (instrumentation/ghidra/mcp_server.ps1).
# It opens a copy of the project with --discard, so nothing that it changes is saved.
# Changes that must stay go into a script or a CSV in the repository, then into the real
# project. Run this after each change to the real project (ApplyTypes.java, ApplyNames.java).
# Changes to LiftExport.java or lift.py do not need it.
# Usage: bash instrumentation/ghidra/mcp_refresh.sh
set -euo pipefail
powershell -NoProfile -ExecutionPolicy Bypass -File "$(dirname "$0")/mcp_server.ps1" refresh
