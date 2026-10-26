@echo off
rem Recreate compiler metadata and objects with one language for dependency parsing.
setlocal
set VSLANG=1033
if not defined VCVARS set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || exit /b 1
cd /d "%~dp0..\.."
cmake --fresh -S . -B build\hook -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DSV_BUILD_INSTRUMENTATION=ON || exit /b 1
cmake --build build\hook || exit /b 1
cmake --fresh -S . -B build\native -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DSV_BUILD_INSTRUMENTATION=OFF || exit /b 1
cmake --build build\native || exit /b 1
