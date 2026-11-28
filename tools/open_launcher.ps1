# Opens the launcher with the options of one mode, so a test starts with the right recording.
#
#   debug   Debug Mode on, preset "Original Recording": the original server and the original client run with
#           the recorder (the client starts with the server), each call goes to build\recordings after the test.
#   normal  Debug Mode off, preset "Normal Playtest": a speed profile and small crash or freeze dumps.
#
# Use: powershell -NoProfile -File tools\open_launcher.ps1 -Mode debug
#      powershell -NoProfile -File tools\open_launcher.ps1 -Mode normal
# Close the launcher first: it writes its settings when it closes. -DryRun only prints the new settings.
param(
    [Parameter(Mandatory = $true)][ValidateSet('debug', 'normal')][string]$Mode,
    [switch]$DryRun
)

$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root 'build\launcher'
$exe = Join-Path $dir 'opensubrosa24_launcher.exe'
$ini = Join-Path $dir 'launcher.ini'

if (-not (Test-Path $exe)) {
    Write-Host "The launcher is not built. Run tools\launcher\build.bat first."
    exit 1
}
if ((-not $DryRun) -and (Get-Process -Name 'opensubrosa24_launcher' -ErrorAction SilentlyContinue)) {
    Write-Host "The launcher is open. Close it and run this again, so that it keeps the new settings."
    exit 1
}

if ($Mode -eq 'debug') {
    $want = [ordered]@{ developer = '1'; preset = 'Original Recording'; client_with_server = '1' }
} else {
    $want = [ordered]@{ developer = '0'; preset = 'Normal Playtest' }
}

$lines = @()
if (Test-Path $ini) { $lines = @(Get-Content $ini) }
foreach ($key in $want.Keys) {
    $found = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match ('^' + [regex]::Escape($key) + '=')) {
            $lines[$i] = $key + '=' + $want[$key]
            $found = $true
            break
        }
    }
    if (-not $found) { $lines += ($key + '=' + $want[$key]) }
}

if ($DryRun) {
    foreach ($key in $want.Keys) { Write-Host ($key + '=' + $want[$key]) }
    exit 0
}

Set-Content -Path $ini -Value $lines -Encoding ASCII
Start-Process -FilePath $exe -WorkingDirectory $dir
Write-Host "Launcher open in $Mode mode."
