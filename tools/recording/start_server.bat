@echo off
rem Start the server for a recording.
rem Put this file in the server folder (next to subrosadedicated024d.exe) and open it.
rem 1. It moves the files of the previous recording into recordings\<date-time>, in this
rem    folder. Nothing is deleted.
rem 2. It starts the server. The hook DLL (wsock32.dll) records while the server runs.
setlocal
cd /d "%~dp0"

if not exist "subrosadedicated024d.exe" (
    echo subrosadedicated024d.exe is not in this folder. Put this file in the server folder.
    pause
    exit /b 1
)

tasklist /fi "imagename eq subrosadedicated024d.exe" | find /i "subrosadedicated024d.exe" >nul
if not errorlevel 1 (
    echo The server is already running. Close it first, then open this file again.
    pause
    exit /b 1
)

set HAS=
for %%f in (sv_args.txt sv_snapshot.bin sv_packets.bin) do if exist "%%f" set HAS=1
if defined HAS (
    for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set STAMP=%%i
    call :save
)
if errorlevel 1 exit /b 1

echo Starting the server...
start "" "subrosadedicated024d.exe"
exit /b 0

:save
set OUT=recordings\%STAMP%
mkdir "%OUT%" || (echo Could not create %OUT%. & pause & exit /b 1)
for %%f in (sv_args.txt sv_snapshot.bin sv_packets.bin subvulpes.log) do (
    if exist "%%f" move "%%f" "%OUT%\" >nul
)
copy "subvulpes.ini" "%OUT%\" >nul 2>nul
echo The previous recording is in %CD%\%OUT%
echo Send that folder to Vulpzin. Do not post it in public: it has player names and chat.
echo.
exit /b 0
