@echo off
cd /d "%~dp0\.."
if "%~1"=="" (
  echo Uso: Compilar-exemplo.bat "C:\caminho\subrosadedicated024d.exe"
  pause
  exit /b 1
)
py -3 tools\build_market_dll.py --cc i686-w64-mingw32-gcc --server "%~1"
if errorlevel 1 (
  echo Precisa de MinGW-w64 i686 no PATH para RECOMPILAR. A DLL pronta ja esta incluida.
  pause
  exit /b 1
)
echo Reinicie o servidor antes de trocar uma DLL ja carregada.
pause
