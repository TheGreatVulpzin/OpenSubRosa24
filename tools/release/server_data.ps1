# Shared checks for local data and release archives.
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
    }
}

function Test-DataEntry([string]$Name) {
    if ($Name -match '[\\:\x00-\x1f]') { return $false }
    return $Name -cmatch '^data/(city\.sbc|block/[^/]+\.sbl|building/[^/]+\.sbb|texture/[^/]+\.png|weapon/[^/]+\.csm)$'
}

function Get-ServerDataFiles([string]$Source) {
    $root = Get-Item -LiteralPath $Source -Force
    if (-not $root.PSIsContainer) { throw 'The data source must be a directory.' }
    Assert-PlainPath $root.FullName
    $files = @(foreach ($item in Get-ChildItem -LiteralPath $root.FullName -Force -Recurse) {
        Assert-PlainPath $item.FullName
        $relative = $item.FullName.Substring($root.FullName.TrimEnd('\', '/').Length + 1).Replace('\', '/')
        if ($item.Name -ieq 'config.txt' -or $item.Extension -in @('.srk', '.exe') -or
            $relative -match '(^|/)user(/|$)') {
            throw "Forbidden data entry: $relative"
        }
        if ($item.PSIsContainer) {
            if ($relative -cnotin @('block', 'building', 'texture', 'weapon')) {
                throw "Unexpected data directory: $relative"
            }
        } else {
            if (-not (Test-DataEntry "data/$relative")) { throw "Unexpected data file: $relative" }
            [pscustomobject]@{ FullName = $item.FullName; Relative = $relative; Length = $item.Length }
        }
    })
    foreach ($pattern in @('city.sbc', 'block/*.sbl', 'building/*.sbb', 'texture/*.png', 'weapon/*.csm')) {
        if (-not @($files | Where-Object Relative -CLike $pattern).Count) {
            throw "Required data is missing: $pattern"
        }
    }
    return $files | Sort-Object Relative
}
