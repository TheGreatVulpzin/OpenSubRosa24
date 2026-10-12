# Record a play session on the test server, for the offline test (src/tests/harness.c).
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\record_session.ps1 [-SnapshotAfter 600]
#
# 1. Copies the current hook DLL to build\auto-server.
# 2. Writes subvulpes.ini: verifier on, argument recording, packet capture, and a
#    snapshot of the memory SnapshotAfter seconds after the start.
# 3. Starts the server in a visible window. Play, then close the server window.
#
# Output in build\auto-server: sv_args.txt, sv_snapshot.bin, sv_packets.bin, subvulpes.log.
# These files have player names and chat: they stay on this PC (build\ is not in git).
param([int]$SnapshotAfter = 600)

$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "build\auto-server"
$dll = Join-Path $root "build\hook\wsock32.dll"
if (-not (Test-Path (Join-Path $dir "subrosadedicated024d.exe"))) { throw "no test server in $dir" }
if (Get-Process subrosadedicated024d -ErrorAction SilentlyContinue) { throw "a server is already running: close it first" }

Copy-Item $dll (Join-Path $dir "wsock32.dll") -Force
@"
[hooks]
enabled=1
verify=1
log_calls=0
lifted=1
record_args=1
snapshot_after=$SnapshotAfter
capture_packets=1
"@ | Set-Content (Join-Path $dir "subvulpes.ini") -Encoding ascii

foreach ($f in "sv_args.txt", "sv_snapshot.bin", "sv_packets.bin") {
    $p = Join-Path $dir $f
    if (Test-Path $p) { Remove-Item $p }
}
Set-Content (Join-Path $dir "subvulpes.log") ""

Write-Host "Server starts now (port in config.txt). The map takes about 3 minutes to load."
Write-Host "The memory snapshot is taken $SnapshotAfter seconds after the start: play at least that long."
Start-Process -FilePath (Join-Path $dir "subrosadedicated024d.exe") -WorkingDirectory $dir
