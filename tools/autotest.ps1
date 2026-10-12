# Automatic server test: run the 024d with the hook DLL for N seconds, then summarize the log.
#
# Uses build\auto-server (a copy of the server on port 27600). Prepare it once with:
#   cp -r original/blackhouser/. build/auto-server/   and set port=27600 in its config.txt
#
# Usage: powershell -File tools\autotest.ps1 [-Seconds 120]
param([int]$Seconds = 120)

$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "build\auto-server"
Copy-Item (Join-Path $root "build\hook\wsock32.dll") (Join-Path $dir "wsock32.dll") -Force
$log = Join-Path $dir "subvulpes.log"
if (Test-Path $log) { Clear-Content $log }

$p = Start-Process -FilePath (Join-Path $dir "subrosadedicated024d.exe") -WorkingDirectory $dir -PassThru -WindowStyle Minimized
$crashed = $false
try {
    Wait-Process -Id $p.Id -Timeout $Seconds -ErrorAction Stop
    $crashed = $true
} catch {
    Stop-Process -Id $p.Id -Force
}

$lines = Get-Content $log
$verified = ($lines | Select-String "VERIFY \S+: 1 verified, 0 differences").Count
$different = $lines | Select-String "VERIFY (\S+): DIFFERENT" | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique
$crashes = $lines | Select-String "^\S+ CRASH (\S+):" | ForEach-Object { $_.Matches[0].Groups[1].Value } | Sort-Object -Unique

if ($crashed) { "SERVER EXITED early (exit code $($p.ExitCode))" } else { "server ran $Seconds s" }
"functions with a first verification and 0 differences: $verified"
"functions with differences ($($different.Count)): $($different -join ', ')"
"functions that crashed ($($crashes.Count)): $($crashes -join ', ')"
