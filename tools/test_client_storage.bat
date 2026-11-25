@echo off
rem Build and run the start value test of the native client storage (instrumentation\tests\test_client_storage.c).
rem Run tools\re\gen_client_native.py first. The image tables need: python -I instrumentation\lift\gen_image.py --client
setlocal
set VSLANG=1033
where cl >nul 2>nul && goto have_cl
if not defined VCVARS set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat
call "%VCVARS%" >nul 2>nul || (echo ERROR: vcvars32.bat not found & exit /b 1)
:have_cl
cd /d "%~dp0.."
if not exist build mkdir build
cl /nologo /std:c11 /arch:IA32 /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /DSR_CLIENT_NATIVE /Isrc\include /Isrc\client /Isrc\platform /Isrc\lifted /Isrc\standalone /Iinstrumentation\client\runtime /Febuild\test_client_storage.exe /Fobuild\ instrumentation\tests\test_client_storage.c src\client\storage.c build\client_image\image_client.c || exit /b 1
build\test_client_storage.exe
