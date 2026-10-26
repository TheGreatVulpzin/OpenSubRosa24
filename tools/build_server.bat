@echo off
rem Build only the normal server, without differential tools or the recording runtime.
setlocal
set VSLANG=1033
where cl >nul 2>nul && goto have_cl
if not defined VCVARS set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || (echo ERROR: vcvars32.bat not found & exit /b 1)
:have_cl
cd /d "%~dp0.."
if not exist build\native\.cmake\api\v1\query mkdir build\native\.cmake\api\v1\query
type nul > build\native\.cmake\api\v1\query\codemodel-v2
cmake -S . -B build\native -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DSV_BUILD_INSTRUMENTATION=OFF || exit /b 1
cmake --build build\native || exit /b 1
echo OK: build\native\opensubrosa24_server.exe
