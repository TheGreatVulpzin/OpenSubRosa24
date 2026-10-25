[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$Checksum,
    [Parameter(Mandatory = $true)][string]$ChangelogFile,
    [string]$Summary,
    [string]$Output
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if ($Version -notmatch '^v\d+\.\d+\.\d+(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?$') {
    throw 'Use a version tag such as v0.1.0 or v0.1.0-rc1.'
}
$line = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $Checksum)).Trim()
if ($line -cnotmatch "^([0-9a-fA-F]{64})  OpenSubRosa24-server-$([regex]::Escape($Version))\.zip$") {
    throw 'The package checksum does not match the version tag.'
}
$hash = $Matches[1].ToLowerInvariant()
if ([string]::IsNullOrWhiteSpace($Summary)) {
    $kind = & git -C $repo for-each-ref '--format=%(objecttype)' "refs/tags/$Version"
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read tag metadata.' }
    if ($LASTEXITCODE -eq 0 -and $kind -eq 'tag') {
        $text = & git -C $repo for-each-ref '--format=%(contents)' "refs/tags/$Version"
        if ($LASTEXITCODE -ne 0) { throw 'Cannot read the annotated tag message.' }
        $Summary = ($text -join "`n").Trim()
    }
}
if ([string]::IsNullOrWhiteSpace($Summary)) {
    $Summary = 'Server package.'
    Write-Warning 'The release summary is empty. Using "Server package.".'
}
$changelog = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $ChangelogFile)).Trim()
# Keep generated changes inside the template section.
$changelog = [regex]::Replace($changelog, '(?m)^#{1,6}\s+.*(?:\r?\n|$)', '').Trim()
$values = @{ version = $Version; summary = $Summary.Trim(); sha256 = $hash; changelog = $changelog }
$template = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'release-notes.md'))
$body = [regex]::Replace($template, '\{(version|summary|sha256|changelog)\}', {
    param($match)
    $values[$match.Groups[1].Value]
})
if ($Version.Contains('-')) { $body = "**Pre-release.** For testing.`n`n$body" }
if (-not $Output) { $Output = Join-Path $repo 'build\package\release-notes.md' }
New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($Output))) -Force | Out-Null
[IO.File]::WriteAllText([IO.Path]::GetFullPath($Output), $body, [Text.UTF8Encoding]::new($false))
return [IO.Path]::GetFullPath($Output)
