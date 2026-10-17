@echo off
rem Builds build\launcher\subvulpes-launcher.exe with the C# compiler of .NET Framework 4.
setlocal
cd /d "%~dp0..\.."
set NET=%WINDIR%\Microsoft.NET\Framework\v4.0.30319
if not exist build\launcher mkdir build\launcher
"%NET%\csc.exe" /nologo /target:winexe /optimize+ /out:build\launcher\subvulpes-launcher.exe ^
    /r:System.Windows.Forms.dll /r:System.Drawing.dll ^
    /r:"%NET%\System.IO.Compression.dll" /r:"%NET%\System.IO.Compression.FileSystem.dll" ^
    tools\launcher\Launcher.cs
