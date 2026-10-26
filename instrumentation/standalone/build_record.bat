@echo off
rem Build the recording executable without rebuilding the differential harness.
setlocal
set VSLANG=1033
where cl >nul 2>nul && goto have_cl
if not defined VCVARS set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || exit /b 1
:have_cl
cd /d "%~dp0..\.."
cmake -S . -B build\hook -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DSV_BUILD_INSTRUMENTATION=ON || exit /b 1
cmake --build build\hook --target subvulpes-server-record || exit /b 1
