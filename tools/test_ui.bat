@echo off
rem Shows the harness banner and lines at several console widths (SV_UI_WIDTH overrides the
rem real width). Usage: tools\test_ui.bat [width ...]   Default: 40 60 100.
rem Needs build\hook\harness.exe (tools\build_hook.bat) and the play-20261012 recording.
setlocal
cd /d "%~dp0.."
set R=build\recordings\play-20261012
set WIDTHS=%*
if "%WIDTHS%"=="" set WIDTHS=40 60 100
for %%W in (%WIDTHS%) do (
    echo.
    echo ================ width %%W ================
    set SV_UI_WIDTH=%%W
    call :run
)
endlocal
exit /b 0

:run
build\hook\harness.exe build\hook\024d_fixed.exe %R%\sv_snapshot.bin %R%\sv_args.txt pack
exit /b 0
