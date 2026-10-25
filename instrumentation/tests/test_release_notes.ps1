$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$script = Join-Path $repo 'tools\release\make_release_notes.ps1'
$directory = Join-Path $repo 'build\package\notes-check'
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$checksum = Join-Path $directory 'package.sha256'
$changelog = Join-Path $directory 'changelog.md'
$output = Join-Path $directory 'body.md'
$hash = 'a' * 64
[IO.File]::WriteAllText($changelog, "## What's Changed`n* Include server data. by @tester in #1`n`n## New Contributors`n* @tester made their first contribution.`n")

function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

foreach ($version in @('v0.0.0', 'v0.0.0-test')) {
    [IO.File]::WriteAllText($checksum, "$hash  OpenSubRosa24-server-$version.zip`n")
    & $script -Version $version -Checksum $checksum -ChangelogFile $changelog -Summary 'Ready for {changelog} testing.' -Output $output | Out-Null
    $body = [IO.File]::ReadAllText($output)
    Assert ($body.Contains('Ready for {changelog} testing.')) 'Summary placeholders must stay literal.'
    Assert ($body.Contains("SHA-256: ``$hash``")) 'Checksum is missing.'
    Assert ($body.Contains("OpenSubRosa24-server-$version.zip")) 'Version is missing from the download name.'
    Assert ($body.Contains('* Include server data. by @tester in #1')) 'Generated change is missing.'
    Assert ($body.Contains('* @tester made their first contribution.')) 'Generated contributor is missing.'
    Assert ($body.StartsWith('Ready for {changelog} testing.')) 'The release body must start with the summary.'
    Assert (-not $body.Contains('## OpenSubRosa24 Server')) 'The release body repeats the title.'
    Assert (-not $body.Contains('**Pre-release.** For testing.')) 'The release body contains a pre-release notice.'
    Assert ([regex]::Matches($body, '(?m)^#{1,6} ').Count -eq 1) 'Unexpected release sections.'
}

# Read tag messages without creating tags in the repository.
function git {
    $global:LASTEXITCODE = 0
    if ($args -contains '--format=%(objecttype)') { return $tagKind }
    return $tagMessage
}
try {
    $script:tagKind = 'tag'
    $script:tagMessage = @('Annotated summary.', '', 'Second paragraph.')
    & $script -Version v0.0.0-test -Checksum $checksum -ChangelogFile $changelog -Output $output | Out-Null
    Assert ([IO.File]::ReadAllText($output).Contains("Annotated summary.`n`nSecond paragraph.")) 'Annotated summary is missing.'
    foreach ($kind in @('commit', '')) {
        $script:tagKind = $kind
        & $script -Version v0.0.0-test -Checksum $checksum -ChangelogFile $changelog -Output $output -WarningVariable warning | Out-Null
        Assert ([IO.File]::ReadAllText($output).Contains('Server package.')) 'Fallback summary is missing.'
        Assert ($warning.Count -eq 1) 'Fallback warning is missing.'
    }
    $script:tagKind = 'tag'
    $script:tagMessage = ''
    & $script -Version v0.0.0-test -Checksum $checksum -ChangelogFile $changelog -Output $output -WarningVariable warning | Out-Null
    Assert ($warning.Count -eq 1) 'Empty tag warning is missing.'
} finally { Remove-Item Function:\git }
Write-Host 'ALL RELEASE NOTE CHECKS PASSED: template, tag summary, manual summary, fallback, checksum, sections, pre-release'
