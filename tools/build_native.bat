@echo off
rem Build targets of the native client. Usage: tools\build_native.bat target [target ...]
rem Targets: subvulpes-client-native, subvulpes-client-native-smoke, subvulpes-client-native-guard,
rem subvulpes-client-native-guard-smoke. The log is build\native_build.log.
rem The exit code is 0 only when the build passes. A build with an error never leaves a 0 here,
rem so no test can run on an old executable by mistake.
setlocal
set VSLANG=1033
where cl >nul 2>nul && goto have_cl
if not defined VCVARS set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || (echo ERROR: vcvars32.bat not found & exit /b 1)
:have_cl
cd /d "%~dp0.."
if not exist build mkdir build
cmake -S . -B build\hook -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DSV_BUILD_INSTRUMENTATION=ON > build\native_cmake.log 2>&1
if errorlevel 1 (
    type build\native_cmake.log
    exit /b 1
)
cmake --build build\hook --target %* > build\native_build.log 2>&1
set code=%errorlevel%
findstr /c:"error C" /c:"error LNK" /c:"fatal error" build\native_build.log > build\native_build_errors.log
if not errorlevel 1 set code=1
if not "%code%"=="0" (
    echo BUILD FAILED, first errors of build\native_build.log:
    type build\native_build_errors.log
)
exit /b %code%
