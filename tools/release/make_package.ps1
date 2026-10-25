[CmdletBinding()]
param([string]$Version)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))

function Read-Git([string[]]$Arguments) {
    $value = & git -C $repo @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read Git metadata.' }
    return ($value -join "`n").Trim()
}

if (-not $Version) {
    if ($env:GITHUB_REF_TYPE -eq 'tag') {
        $Version = $env:GITHUB_REF_NAME
    } else {
        $Version = Read-Git -Arguments @('describe', '--tags', '--exact-match', 'HEAD')
    }
}
if ($Version -notmatch '^v?\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?$') {
    throw 'Use a version such as v0.1.0 or v0.1.0-rc1.'
}
if (-not $Version.StartsWith('v')) { $Version = 'v' + $Version }
$commit = Read-Git -Arguments @('rev-parse', '--short=12', 'HEAD')
$date = [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
$name = "OpenSubRosa24-server-$Version"
$output = Join-Path $repo 'build\package'
$package = Join-Path $output $name
$zip = Join-Path $output "$name.zip"
$checksum = "$zip.sha256"
$sources = [ordered]@{
    'subvulpes-server.exe' = 'build\hook\subvulpes-server.exe'
    'subvulpes-server-record.exe' = 'build\hook\subvulpes-server-record.exe'
    'subvulpes-launcher.exe' = 'build\launcher\subvulpes-launcher.exe'
    'LEIA-ME.txt' = 'tools\release\LEIA-ME.txt'
    'record_focus.txt' = 'instrumentation\recording\record_focus.txt'
}
$allowed = @($sources.Keys) + @('version.txt')

function Assert-PlainPath([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force
    if (-not $item.PSIsContainer) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Links are not allowed in package paths: $Path"
        }
        $item = $item.Directory
    }
    while ($null -ne $item) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Links are not allowed in package paths: $Path"
        }
        $item = $item.Parent
        if ($null -eq $item) { break }
    }
}

# Copy only these sources. Never copy a server installation or a directory tree.
foreach ($source in $sources.Values) {
    $path = Join-Path $repo $source
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Build or source file is missing: $source"
    }
    Assert-PlainPath $path
}
New-Item -ItemType Directory -Path $package -Force | Out-Null
Assert-PlainPath $package

function Assert-Package {
    $items = @(Get-ChildItem -LiteralPath $package -Force -Recurse)
    foreach ($item in $items) {
        if ($item.PSIsContainer -or $item.Name -cnotin $allowed -or
            $item.DirectoryName -ne $package -or
            ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Forbidden package entry: $($item.Name)"
        }
        if ($item.Extension -eq '.txt') {
            $text = [IO.File]::ReadAllText($item.FullName)
            if ($text -match '(?im)^\s*(?:serverpassword|password|passwd|token|secret|api_key)\s*[=:]\s*\S+' -or
                $text -match '-----BEGIN (?:RSA |OPENSSH |EC )?PRIVATE KEY-----|gh[pousr]_[A-Za-z0-9]{20,}') {
                throw "Private data detected in package text: $($item.Name)"
            }
        }
    }
}

# Refuse contamination from an earlier run instead of silently hiding it.
Assert-Package
foreach ($entry in $sources.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $repo $entry.Value) -Destination (Join-Path $package $entry.Key) -Force
}
[IO.File]::WriteAllText((Join-Path $package 'version.txt'),
    "$Version`ncommit $commit`nbuilt $date`n", [Text.UTF8Encoding]::new($false))
Assert-Package
if (@(Get-ChildItem -LiteralPath $package -Force).Count -ne $allowed.Count) {
    throw 'The package file count is incorrect.'
}

# Reject known game files even if someone renames them as a package executable.
$gameHashes = @(Get-Content -LiteralPath (Join-Path $repo 'original\MANIFEST.sha256') |
    ForEach-Object { if ($_ -match '^([0-9a-fA-F]{64})\s') { $Matches[1] } })
foreach ($file in Get-ChildItem -LiteralPath $package -File) {
    if ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -in $gameHashes) {
        throw "A game file cannot enter the package: $($file.Name)"
    }
    if ($file.Extension -eq '.exe') {
        $bytes = [IO.File]::ReadAllBytes($file.FullName)
        if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x4d -or $bytes[1] -ne 0x5a) {
            throw "Invalid executable: $($file.Name)"
        }
        $offset = [BitConverter]::ToInt32($bytes, 0x3c)
        if ($offset -lt 0 -or $offset -gt ($bytes.Length - 6) -or
            [BitConverter]::ToUInt32($bytes, $offset) -ne 0x4550 -or
            [BitConverter]::ToUInt16($bytes, $offset + 4) -ne 0x14c) {
            throw "Expected an x86 PE executable: $($file.Name)"
        }
    }
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
foreach ($path in @($zip, $checksum)) {
    if (Test-Path -LiteralPath $path) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Expected an output file: $path"
        }
        Assert-PlainPath $path
        Remove-Item -LiteralPath $path
    }
}
$archive = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in $allowed) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,
            (Join-Path $package $file), "$name/$file", [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally {
    $archive.Dispose()
}
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $expected = @($allowed | ForEach-Object { "$name/$_" } | Sort-Object)
    $actual = @($archive.Entries | ForEach-Object { $_.FullName } | Sort-Object)
    if (Compare-Object $expected $actual) { throw 'The zip contains unexpected entries.' }
} finally {
    $archive.Dispose()
}
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($checksum, "$hash  $name.zip`n", [Text.Encoding]::ASCII)
Write-Host "Package: $package"
Write-Host "SHA256: $hash"
[pscustomobject]@{ Version = $Version; Package = $package; Zip = $zip; Checksum = $checksum }
