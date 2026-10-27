# Run after both build scripts. All test artifacts stay in build/package/.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$script = Join-Path $repo 'tools\release\make_package.ps1'
$version = 'v0.0.0-package-data-check'
$fixture = Join-Path $repo 'build\package\data-fixture'
foreach ($name in @('city.sbc', 'block/test.sbl', 'building/test.sbb', 'texture/test.png', 'weapon/test.csm')) {
    $path = Join-Path $fixture $name
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($path)) -Force | Out-Null
    [IO.File]::WriteAllText($path, 'package test fixture')
}
$package = & $script -Version $version -DataDir $fixture
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
    Assert ($archive.Entries.Count -eq 12) 'Expected seven root files and five data files.'
    Assert ($null -eq $archive.GetEntry("OpenSubRosa24-server-$version/config.txt")) 'The package must not include config.txt.'
    $executables = @($archive.Entries | Where-Object { $_.Name.EndsWith('.exe') } | ForEach-Object Name | Sort-Object)
    $expectedExecutables = @('opensubrosa24_server.exe', 'opensubrosa24_server_record.exe', 'opensubrosa24_launcher.exe') | Sort-Object
    Assert (($executables -join ',') -ceq ($expectedExecutables -join ',')) 'The package executable names must use opensubrosa24.'
    foreach ($entry in $archive.Entries) {
        $relative = $entry.FullName.Substring("OpenSubRosa24-server-$version/".Length)
        Assert ($entry.Name -cin $names -or $relative -ceq 'readme.txt' -or
            $relative.StartsWith('data/')) "Unexpected zip entry: $relative"
        Write-Host "ZIP: $($entry.FullName) ($($entry.Length) bytes)"
    }
    foreach ($name in $names) {
        Assert ($null -ne $archive.GetEntry("OpenSubRosa24-server-$version/$name")) "Missing launcher update file: $name"
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

foreach ($name in @('server.srk', 'sv_snapshot.bin', 'subrosadedicated024d.exe')) {
    $path = Join-Path $package.Package $name
    [IO.File]::WriteAllText($path, 'package test fixture')
    try {
        Expect-Rejection $name { & $script -Version $version -DataDir $fixture } 'Forbidden package entry:*'
    } finally { Remove-Item -LiteralPath $path }
}
$data = Join-Path $package.Package 'user'
New-Item -ItemType Directory -Path $data | Out-Null
try {
    Expect-Rejection 'account directory' { & $script -Version $version -DataDir $fixture } 'Forbidden package entry:*'
} finally { Remove-Item -LiteralPath $data }
$readme = Join-Path $package.Package 'LEIA-ME.txt'
$saved = [IO.File]::ReadAllBytes($readme)
try {
    [IO.File]::WriteAllText($readme, 'serverpassword=package-test-value')
    Expect-Rejection 'password assignment' { & $script -Version $version -DataDir $fixture } 'Private data detected*'
} finally { [IO.File]::WriteAllBytes($readme, $saved) }
$config = Join-Path $package.Package 'config.txt'
try {
    [IO.File]::WriteAllText($config, 'serverpassword=package-test-value')
    Expect-Rejection 'stray config' { & $script -Version $version -DataDir $fixture } 'Forbidden package entry*'
} finally { Remove-Item -LiteralPath $config -ErrorAction SilentlyContinue }
Expect-Rejection 'version path traversal' { & $script -Version '../invalid' -DataDir $fixture } 'Use a version*'
Expect-Rejection 'missing data argument' { & $script -Version $version } 'Pass -DataDir*'

foreach ($name in @('config.txt', 'server.srk', 'forbidden.exe', 'user/account.txt')) {
    $path = Join-Path $fixture $name
    New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($path)) -Force | Out-Null
    [IO.File]::WriteAllText($path, 'package test fixture')
    try {
        Expect-Rejection "data source $name" { & $script -Version $version -DataDir $fixture } 'Forbidden data entry:*'
        Expect-Rejection "data archive $name" { & (Join-Path $repo 'tools\release\make_data_zip.ps1') -Source $fixture } 'Forbidden data entry:*'
    } finally {
        Remove-Item -LiteralPath $path
        if ($name.StartsWith('user/')) { Remove-Item -LiteralPath (Join-Path $fixture 'user') }
    }
}

$dataZip = & (Join-Path $repo 'tools\release\make_data_zip.ps1') -Source $fixture
$destination = Join-Path $repo ('build\package\data-extract-' + [guid]::NewGuid().ToString('N'))
$expanded = & (Join-Path $repo 'tools\release\expand_data_zip.ps1') -Zip $dataZip.Zip -Checksum $dataZip.Checksum -Destination $destination
Assert ((Get-ChildItem -LiteralPath $expanded -Recurse -File).Count -eq 5) 'Data extraction failed.'
$savedHash = [IO.File]::ReadAllBytes($dataZip.Checksum)
try {
    [IO.File]::WriteAllText($dataZip.Checksum, (('0' * 64) + '  server-data-0.24.zip'))
    Expect-Rejection 'data checksum mismatch' {
        & (Join-Path $repo 'tools\release\expand_data_zip.ps1') -Zip $dataZip.Zip -Checksum $dataZip.Checksum -Destination ($destination + '-bad')
    } 'SHA-256 mismatch*'
} finally { [IO.File]::WriteAllBytes($dataZip.Checksum, $savedHash) }

foreach ($entryName in @('data/../config.txt', 'data/block/..\..\config.txt.sbl', 'data/block/secret.exe', 'data/city.sbc')) {
    $malicious = Join-Path $repo 'build\package\unsafe-data.zip'
    if (Test-Path -LiteralPath $malicious) { Remove-Item -LiteralPath $malicious }
    $archive = [IO.Compression.ZipFile]::Open($malicious, [IO.Compression.ZipArchiveMode]::Create)
    try {
        $archive.CreateEntry($entryName) | Out-Null
        if ($entryName -eq 'data/city.sbc') { $archive.CreateEntry($entryName) | Out-Null }
    } finally { $archive.Dispose() }
    $maliciousHash = "$malicious.sha256"
    [IO.File]::WriteAllText($maliciousHash, ((Get-FileHash -LiteralPath $malicious).Hash + '  server-data-0.24.zip'))
    Expect-Rejection "data zip entry $entryName" {
        & (Join-Path $repo 'tools\release\expand_data_zip.ps1') -Zip $malicious -Checksum $maliciousHash -Destination ($destination + '-unsafe')
    } 'Forbidden data zip entry:*'
}
Write-Host 'PASS: data archive round trip'
$passed++

$oldType = $env:GITHUB_REF_TYPE
$oldName = $env:GITHUB_REF_NAME
try {
    $env:GITHUB_REF_TYPE = 'tag'
    $env:GITHUB_REF_NAME = $version
    $fromTag = & $script -DataDir $fixture
    Assert ($fromTag.Version -eq $version) 'Actions tag fallback failed.'
    Write-Host 'PASS: Actions tag version fallback'
    $passed++
} finally {
    $env:GITHUB_REF_TYPE = $oldType
    $env:GITHUB_REF_NAME = $oldName
}
Write-Host "ALL $passed PACKAGE CHECKS PASSED"
