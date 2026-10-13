@echo off
rem Build the hook DLL (wsock32.dll) for x86 with MSVC.
rem Usage: tools\build_hook.bat   (output in build\hook\wsock32.dll)
rem When cl is already in PATH (a "x86 Native Tools" prompt or CI), vcvars32.bat is not called.
rem Set VCVARS to use another Visual Studio.
setlocal
where cl >nul 2>nul && goto have_cl
if not defined VCVARS set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || (echo ERROR: vcvars32.bat not found & exit /b 1)
:have_cl
cd /d "%~dp0.."
cmake -S . -B build\hook -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release || exit /b 1
cmake --build build\hook || exit /b 1
echo.
echo OK: build\hook\wsock32.dll
