@echo off
cd /d "%~dp0"
where py >nul 2>nul
if not errorlevel 1 (
  py -3 manager.py
) else (
  python manager.py
)
if errorlevel 1 (
  echo.
  echo Precisa de Python 3.10 ou mais recente, com Tcl/Tk, no Windows.
  echo Veja LEIA-ME.md para instrucoes.
  pause
)
