@echo off
rem Build the hook DLL (wsock32.dll) for x86 with MSVC.
rem Usage: tools\build_hook.bat   (output in build\hook\wsock32.dll)
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || (echo ERROR: vcvars32.bat not found & exit /b 1)
cd /d "%~dp0.."
cmake -S . -B build\hook -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build\hook || exit /b 1
echo.
echo OK: build\hook\wsock32.dll
