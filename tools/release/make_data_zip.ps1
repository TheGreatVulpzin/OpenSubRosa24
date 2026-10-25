[CmdletBinding()]
param([Parameter(Mandatory = $true)][string]$Source)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'server_data.ps1')
$files = @(Get-ServerDataFiles $Source)
$output = Join-Path $PSScriptRoot '..\..\build\package\data-release'
New-Item -ItemType Directory -Path $output -Force | Out-Null
Assert-PlainPath $output
$zip = [IO.Path]::GetFullPath((Join-Path $output 'server-data-0.24.zip'))
$checksum = "$zip.sha256"
foreach ($path in @($zip, $checksum)) {
    if (Test-Path -LiteralPath $path) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Expected an output file: $path" }
        Assert-PlainPath $path
        Remove-Item -LiteralPath $path
    }
}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in $files) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName,
            "data/$($file.Relative)", [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $archive.Dispose() }
$archive = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $expected = @($files | ForEach-Object { "data/$($_.Relative)" })
    $actual = @($archive.Entries | ForEach-Object { $_.FullName })
    if (Compare-Object $expected $actual) { throw 'The data zip contains unexpected entries.' }
} finally { $archive.Dispose() }
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($checksum, "$hash  server-data-0.24.zip`n", [Text.Encoding]::ASCII)
Write-Host "Data: $($files.Count) files, $(($files | Measure-Object Length -Sum).Sum) bytes"
Write-Host "Zip: $zip ($((Get-Item -LiteralPath $zip).Length) bytes)"
Write-Host "SHA256: $hash"
[pscustomobject]@{ Zip = $zip; Checksum = $checksum }
