param([switch]$CompileOnly)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
Push-Location $repo
try {
    & cmd /c tools\launcher\build.bat
    if ($LASTEXITCODE -ne 0) { throw 'Launcher build failed.' }
    $framework = Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319'
    & (Join-Path $framework 'csc.exe') /nologo /target:exe /platform:x86 /main:SubVulpes.LauncherTests `
        /out:build\launcher\launcher-tests.exe /r:System.Windows.Forms.dll /r:System.Drawing.dll `
        /r:System.Core.dll /r:System.Web.Extensions.dll "/r:$framework\System.IO.Compression.dll" `
        "/r:$framework\System.IO.Compression.FileSystem.dll" `
        /resource:tools\release\default-config.ini,SubVulpes.DefaultConfig `
        tools\launcher\Launcher.cs instrumentation\tests\LauncherTests.cs
    if ($LASTEXITCODE -ne 0) { throw 'Launcher test build failed.' }
    if (-not $CompileOnly) {
        & build/launcher/launcher-tests.exe $repo
        if ($LASTEXITCODE -ne 0) { throw 'Launcher tests failed.' }
    }
} finally { Pop-Location }
