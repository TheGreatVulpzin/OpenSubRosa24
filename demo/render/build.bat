@echo off
rem Builds build\demo\render_demo.exe. Uses the same Visual Studio as tools\build_hook.bat.
setlocal
where cl >nul 2>nul
if not errorlevel 1 goto build
if not defined VCVARS set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
call "%VCVARS%" >nul 2>nul || (echo ERROR: vcvars32.bat not found & exit /b 1)
:build
set "ROOT=%~dp0..\.."
if not exist "%ROOT%\build\demo" mkdir "%ROOT%\build\demo"
cl /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS "%~dp0render_demo.c" /Fo"%ROOT%\build\demo\\" /Fe"%ROOT%\build\demo\render_demo.exe" /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib opengl32.lib || (echo build failed & exit /b 1)
copy /y "%~dp0passes.txt" "%ROOT%\build\demo\" >nul
echo built build\demo\render_demo.exe
