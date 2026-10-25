# Record a play session on the test server, for the offline test (instrumentation/tests/harness.c).
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\record_session.ps1 [-SnapshotAfter 600]
#
# 1. Copies the current hook DLL to build\auto-server.
# 2. Writes subvulpes.ini: only the original code runs (original_only=1, no C version,
#    no verification), with argument recording, packet capture, and a
#    snapshot of the memory SnapshotAfter seconds after the start.
# 3. Starts the server in a visible window. Play, then close the server window.
#
# Output in build\auto-server: sv_args.txt, sv_snapshot.bin, sv_packets.bin, subvulpes.log.
# The files of the previous recording move to build\recordings\<date-time> (nothing is deleted).
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
verify=0
original_only=1
log_calls=0
lifted=1
record_args=1
record_imports=1
snapshot_after=$SnapshotAfter
capture_packets=0
"@ | Set-Content (Join-Path $dir "subvulpes.ini") -Encoding ascii
# The functions that keep their hook all the time (instrumentation/hook/record.c, "what to record").
Copy-Item (Join-Path $root "instrumentation\recording\record_focus.txt") (Join-Path $dir "record_focus.txt") -Force

# Keep the files of the previous recording: move them to build\recordings\<date-time>.
$old = Join-Path $root ("build\recordings\" + (Get-Date -Format "yyyyMMdd-HHmmss"))
foreach ($f in "sv_args.txt", "sv_snapshot.bin", "sv_packets.bin", "subvulpes.log") {
    $p = Join-Path $dir $f
    if (Test-Path $p) {
        New-Item -ItemType Directory -Force $old | Out-Null
        Move-Item $p $old
    }
}
if (Test-Path $old) { Write-Host "Previous recording moved to $old" }

Write-Host "Server starts now (port in config.txt). Only the original code runs: it loads like the normal server."
Write-Host "The memory snapshot is taken $SnapshotAfter seconds after the start: play at least that long."
Start-Process -FilePath (Join-Path $dir "subrosadedicated024d.exe") -WorkingDirectory $dir
