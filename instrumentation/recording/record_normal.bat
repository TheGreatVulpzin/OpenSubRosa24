@echo off
rem Start the server with the ORIGINAL game code and record it (SubVulpes).
rem When the server window closes, save_last_record.bat saves the recording by itself.
rem Do not close this black window while the server runs.
setlocal
cd /d "%~dp0"
set NAME=normal
set ORIGINAL_ONLY=1
set OWN_MAIN=0

if not exist "subrosadedicated024d.exe" (
    echo subrosadedicated024d.exe is not in this folder. Put these files in the server folder.
    pause
    exit /b 1
)
tasklist /fi "imagename eq subrosadedicated024d.exe" | "%SystemRoot%\System32\find.exe" /i "subrosadedicated024d.exe" >nul
if not errorlevel 1 (
    echo The server is already running. Close it first, then open this file again.
    pause
    exit /b 1
)
for %%f in (sv_args.txt sv_snapshot.bin sv_packets.bin) do if exist "%%f" (
    echo A recording from before is still here. Saving it first.
    call "%~dp0save_last_record.bat" before
    goto :start
)

:start
rem config.txt can be edited freely. Notepad can save it as "UTF-8 with BOM" or "Unicode",
rem and then the game does not read the first line. Write it back as plain ANSI text.
powershell -NoProfile -Command "$p='config.txt'; if (Test-Path $p) { $b=[IO.File]::ReadAllBytes($p); if ($b.Length -ge 2 -and (($b[0] -eq 0xFF -and $b[1] -eq 0xFE) -or ($b[0] -eq 0xFE -and $b[1] -eq 0xFF) -or ($b.Length -ge 3 -and $b[0] -eq 0xEF -and $b[1] -eq 0xBB -and $b[2] -eq 0xBF))) { $t=[IO.File]::ReadAllText($p); [IO.File]::WriteAllText($p, $t, [Text.Encoding]::Default); Write-Host 'config.txt: encoding fixed (plain text again)' } }"

(
    echo [hooks]
    echo enabled=1
    echo verify=0
    echo original_only=%ORIGINAL_ONLY%
    echo own_main=%OWN_MAIN%
    echo log_calls=0
    echo lifted=1
    echo record_args=1
    echo record_imports=1
    echo snapshot_after=300
    echo capture_packets=0
    echo net_guard=1
) > subvulpes.ini

echo.
echo Mode: %NAME%. The server starts now.
echo Play at least 5 minutes: the memory snapshot is written after 300 seconds.
echo Do NOT close this black window. Close the SERVER window when you finish:
echo this window then saves the recording by itself.
echo.
start "" /wait "subrosadedicated024d.exe"
call "%~dp0save_last_record.bat" %NAME%
exit /b 0
