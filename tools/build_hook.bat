@echo off
rem Compila o DLL de hooks (wsock32.dll) para x86 com MSVC.
rem Uso: tools\build_hook.bat   (saída em build\hook\wsock32.dll)
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || (echo ERRO: vcvars32.bat nao encontrado & exit /b 1)
cd /d "%~dp0.."
cmake -S . -B build\hook -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build\hook || exit /b 1
echo.
echo OK: build\hook\wsock32.dll
