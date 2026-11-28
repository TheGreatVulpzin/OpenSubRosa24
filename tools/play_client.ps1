# Opens the original client with the recorder, in normal mode or in debug mode, and keeps the recording.
#
#   normal  the game as a player sees it, with the recorder on.
#   debug   the same, with the debug flag of the game on (subvulpes.ini [client] debugit=1): the main menu shows
#           Editor, and the debug keys work (= for help, V, F6 and F11 for the displays, Insert for money).
#
# Use: powershell -NoProfile -File tools\play_client.ps1 -Mode normal
#      powershell -NoProfile -File tools\play_client.ps1 -Mode debug
#
# The script copies the recording wsock32.dll and writes subvulpes.ini and record_focus.txt into the client
# folder (build\client_test), starts subrosa_fixed.exe, waits until you close the game, and moves the
# recording to build\recordings\client-play-<time>[-debug]. The files of an older recording go to
# recordings_before\<time> in the client folder: nothing is deleted.
# -Folder uses another client folder, -Snapshot sets the seconds before the memory snapshot (default 240),
# -DryRun only says what it would do.
param(
    [Parameter(Mandatory = $true)][ValidateSet('normal', 'debug')][string]$Mode,
    [string]$Folder = '',
    [int]$Snapshot = 240,
    [switch]$DryRun
)

$root = Split-Path -Parent $PSScriptRoot
if ($Folder -eq '') { $Folder = Join-Path $root 'build\client_test' }
$exe = Join-Path $Folder 'subrosa_fixed.exe'
$proxy = Join-Path $root 'build\hook\client\wsock32.dll'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$suffix = ''
if ($Mode -eq 'debug') { $suffix = '-debug' }
$saveDir = Join-Path $root ('build\recordings\client-play-' + $stamp + $suffix)
$files = @('sv_args.txt', 'sv_snapshot.bin', 'sv_packets.bin', 'subvulpes.log')

if (-not (Test-Path $exe)) { Write-Host "No subrosa_fixed.exe in $Folder"; exit 1 }
if (-not (Test-Path $proxy)) { Write-Host "No recording proxy at $proxy (build the cl_wsock32 target: tools\build_hook.bat)"; exit 1 }
if (Get-Process -Name 'subrosa_fixed' -ErrorAction SilentlyContinue) { Write-Host 'The client is already open. Close it first.'; exit 1 }

$ini = "[hooks]`r`nhooks=1`r`nlifted=1`r`noriginal_only=1`r`nverify=0`r`nrecord_args=1`r`nrecord_imports=1`r`n" +
       "snapshot_after=$Snapshot`r`nrecord_total_mb=100`r`n"
if ($Mode -eq 'debug') { $ini += "`r`n[client]`r`ndebugit=1`r`n" }

if ($DryRun) {
    Write-Host "Folder: $Folder"
    Write-Host "Mode: $Mode"
    Write-Host "Recording goes to: $saveDir"
    Write-Host "subvulpes.ini:"
    Write-Host $ini
    exit 0
}

# The files of an older recording move away first.
$before = Join-Path $Folder ('recordings_before\' + $stamp)
foreach ($name in $files) {
    $path = Join-Path $Folder $name
    if (Test-Path $path) {
        New-Item -ItemType Directory -Force -Path $before | Out-Null
        Move-Item -Path $path -Destination (Join-Path $before $name)
    }
}
Remove-Item -Path (Join-Path $Folder 'snapshot_now.txt') -ErrorAction SilentlyContinue
Copy-Item -Path $proxy -Destination (Join-Path $Folder 'wsock32.dll') -Force
Set-Content -Path (Join-Path $Folder 'subvulpes.ini') -Value $ini -Encoding ASCII -NoNewline

# The functions that the recordings still miss stay hooked all the time.
Push-Location $root
python -I instrumentation\recording\coverage.py --side client --focus $Folder | Out-Null
Pop-Location

Write-Host "Client open in $Mode mode with the recorder. Play for a few minutes, then close the game."
Write-Host 'The memory snapshot is written after about 4 minutes of play, or at once with a file snapshot_now.txt in the client folder.'
$process = Start-Process -FilePath $exe -WorkingDirectory $Folder -PassThru -Wait

New-Item -ItemType Directory -Force -Path $saveDir | Out-Null
foreach ($name in $files + @('subvulpes.ini')) {
    $path = Join-Path $Folder $name
    if (Test-Path $path) { Move-Item -Path $path -Destination (Join-Path $saveDir $name) }
}
Write-Host ('The game closed with exit code ' + $process.ExitCode + '. The recording is in ' + $saveDir)
Write-Host 'Next: python -I instrumentation\lift\min_calls.py --target client_win --only client-play'
