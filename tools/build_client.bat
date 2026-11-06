@echo off
rem Build the standalone client (Phase 7.5) into build\hook\opensubrosa24_client.exe.
rem Run the client pipeline first: it makes build\lift\client_win\lifted_client.c.
setlocal
set VSLANG=1033
where cl >nul 2>nul && goto have_cl
if not defined VCVARS set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || (echo ERROR: vcvars32.bat not found & exit /b 1)
:have_cl
cd /d "%~dp0.."
python -I instrumentation\lift\gen_image.py --client || exit /b 1
python -I instrumentation\client\gen_gl_slots.py || exit /b 1
cmake -S . -B build\hook -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DSV_BUILD_INSTRUMENTATION=ON || exit /b 1
cmake --build build\hook --target subvulpes-client || exit /b 1
echo OK: build\hook\opensubrosa24_client.exe
