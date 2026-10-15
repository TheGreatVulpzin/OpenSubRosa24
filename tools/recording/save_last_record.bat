@echo off
rem Save the last recording: move sv_args.txt, sv_snapshot.bin, sv_packets.bin and
rem subvulpes.log into recordings\<date-time>, and make a .zip of that folder.
rem record_normal.bat and record_subvulpescode.bat call this file when the server closes.
rem Open it by hand when the server window was closed some other way (or the PC crashed).
rem Usage: save_last_record.bat [name]   (name goes after the date: normal, subvulpescode)
setlocal
cd /d "%~dp0"

set HAS=
for %%f in (sv_args.txt sv_snapshot.bin sv_packets.bin) do if exist "%%f" set HAS=1
if not defined HAS (
    echo There is no recording to save in this folder.
    pause
    exit /b 0
)

for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set STAMP=%%i
set OUT=recordings\%STAMP%
if not "%~1"=="" set OUT=recordings\%STAMP%-%~1
mkdir "%OUT%" || (echo Could not create %OUT%. & pause & exit /b 1)
for %%f in (sv_args.txt sv_snapshot.bin sv_packets.bin subvulpes.log) do (
    if exist "%%f" move "%%f" "%OUT%\" >nul
)
copy "subvulpes.ini" "%OUT%\" >nul 2>nul
if not exist "%OUT%\sv_snapshot.bin" echo WARNING: no sv_snapshot.bin. The server closed before 5 minutes, so this recording cannot be tested.

echo Making %OUT%.zip ...
powershell -NoProfile -Command "Compress-Archive -Path '%OUT%\*' -DestinationPath '%OUT%.zip' -Force"
echo.
echo Saved: %CD%\%OUT%.zip
echo Send that .zip to Vulpzin. Do not post it in public: it has player names and chat.
echo.
pause
exit /b 0
