# Run after both build scripts. All test artifacts stay in build/package/.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$script = Join-Path $repo 'tools\release\make_package.ps1'
$version = 'v0.0.0-package-check'
$package = & $script -Version $version
$passed = 0

function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Expect-Rejection([string]$Label, [scriptblock]$Action, [string]$Message) {
    $rejected = $false
    try { & $Action | Out-Null } catch {
        Assert ($_.Exception.Message -like $Message) "Unexpected failure for $Label`: $($_.Exception.Message)"
        $rejected = $true
    }
    Assert $rejected "Package accepted $Label"
    Write-Host "PASS: rejects $Label"
    $script:passed++
}

$archive = [IO.Compression.ZipFile]::OpenRead($package.Zip)
try {
    $launcher = [IO.File]::ReadAllText((Join-Path $repo 'tools\launcher\Launcher.cs'))
    $list = [regex]::Match($launcher, '(?s)string\[\] UpdateFiles\s*=\s*\{(.*?)\}').Groups[1].Value
    $names = @([regex]::Matches($list, '"([^"]+)"') | ForEach-Object { $_.Groups[1].Value })
    Assert ($archive.Entries.Count -eq 6) 'Expected six zip entries.'
    foreach ($entry in $archive.Entries) {
        Assert ($entry.Name -cin $names) "The launcher cannot recognize $($entry.Name)."
        Assert ($entry.FullName -ceq "OpenSubRosa24-server-$version/$($entry.Name)") 'Unexpected zip path.'
        Write-Host "ZIP: $($entry.FullName) ($($entry.Length) bytes)"
    }
    $entry = $archive.Entries | Where-Object Name -eq 'version.txt'
    $reader = [IO.StreamReader]::new($entry.Open())
    try { $metadata = $reader.ReadToEnd() } finally { $reader.Dispose() }
    Assert ($metadata -match "(?m)^$([regex]::Escape($version))$") 'Version tag is missing.'
    $commit = (& git -C $repo rev-parse --short=12 HEAD).Trim()
    Assert ($LASTEXITCODE -eq 0 -and $metadata.Contains("commit $commit")) 'Commit does not match HEAD.'
    Assert ($metadata -match '(?m)^built \d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z') 'UTC date is missing.'
    Write-Host "VERSION:`n$metadata"
} finally { $archive.Dispose() }
$hash = (Get-FileHash -LiteralPath $package.Zip -Algorithm SHA256).Hash.ToLowerInvariant()
Assert ([IO.File]::ReadAllText($package.Checksum).Trim() -eq "$hash  $([IO.Path]::GetFileName($package.Zip))") 'Checksum mismatch.'
Write-Host 'PASS: zip, launcher names, metadata and checksum'
$passed++

foreach ($name in @('config.txt', 'sv_snapshot.bin', 'subrosadedicated024d.exe')) {
    $path = Join-Path $package.Package $name
    [IO.File]::WriteAllText($path, 'package test fixture')
    try {
        Expect-Rejection $name { & $script -Version $version } 'Forbidden package entry:*'
    } finally { Remove-Item -LiteralPath $path }
}
$data = Join-Path $package.Package 'data'
New-Item -ItemType Directory -Path $data | Out-Null
try {
    Expect-Rejection 'game data directory' { & $script -Version $version } 'Forbidden package entry:*'
} finally { Remove-Item -LiteralPath $data }
$readme = Join-Path $package.Package 'LEIA-ME.txt'
$saved = [IO.File]::ReadAllBytes($readme)
try {
    [IO.File]::WriteAllText($readme, 'serverpassword=package-test-value')
    Expect-Rejection 'password assignment' { & $script -Version $version } 'Private data detected*'
} finally { [IO.File]::WriteAllBytes($readme, $saved) }
Expect-Rejection 'version path traversal' { & $script -Version '../invalid' } 'Use a version*'

$oldType = $env:GITHUB_REF_TYPE
$oldName = $env:GITHUB_REF_NAME
try {
    $env:GITHUB_REF_TYPE = 'tag'
    $env:GITHUB_REF_NAME = $version
    $fromTag = & $script
    Assert ($fromTag.Version -eq $version) 'Actions tag fallback failed.'
    Write-Host 'PASS: Actions tag version fallback'
    $passed++
} finally {
    $env:GITHUB_REF_TYPE = $oldType
    $env:GITHUB_REF_NAME = $oldName
}
Write-Host "ALL $passed PACKAGE CHECKS PASSED"
