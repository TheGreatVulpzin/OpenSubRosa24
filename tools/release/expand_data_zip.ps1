[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Zip,
    [Parameter(Mandatory = $true)][string]$Checksum,
    [Parameter(Mandatory = $true)][string]$Destination
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot 'server_data.ps1')
$line = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $Checksum)).Trim()
if ($line -cnotmatch '^([0-9a-fA-F]{64})  server-data-0\.24\.zip$') {
    throw 'Invalid SHA-256 file for server-data-0.24.zip.'
}
$expected = $Matches[1]
if ((Get-FileHash -LiteralPath $Zip -Algorithm SHA256).Hash -ine $expected) {
    throw 'SHA-256 mismatch for server-data-0.24.zip.'
}
if (Test-Path -LiteralPath $Destination) { throw 'The data destination must not exist.' }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $Zip))
try {
    $seen = @{}
    foreach ($entry in $archive.Entries) {
        if (-not (Test-DataEntry $entry.FullName) -or $seen.ContainsKey($entry.FullName)) {
            throw "Forbidden data zip entry: $($entry.FullName)"
        }
        $seen[$entry.FullName] = $true
    }
} finally { $archive.Dispose() }
New-Item -ItemType Directory -Path $Destination | Out-Null
Assert-PlainPath $Destination
[IO.Compression.ZipFile]::ExtractToDirectory((Resolve-Path -LiteralPath $Zip), [IO.Path]::GetFullPath($Destination))
$data = Join-Path ([IO.Path]::GetFullPath($Destination)) 'data'
$files = @(Get-ServerDataFiles $data)
Write-Host "SHA-256 checked. Extracted $($files.Count) data files."
return $data
