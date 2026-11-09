<#
Make a git worktree of the project that can build and test the client, next to the main folder.

The build folder (build/) is not in git. A worktree needs its own outputs (build/hook, build/gate,
build/lift/client_win) and the inputs that tests only read:
  original/<entry>                   junction to each entry of the main folder
  build/recordings                   junction to the same target as the main folder
  build/lift/client_win.jsonl        copy (the Ghidra export)
  build/client_test, build/client_deps   copies (the game folder of the tests)
  build/hook/subrosa_fixed.exe       copy (the client with the base address 0x20000000)

Usage (PowerShell):
  powershell -File tools\client_worktree.ps1 -Path E:\SubVulpes-wt-a -Branch client/c2
Then build and test in the new folder:
  cd E:\SubVulpes-wt-a
  bash instrumentation/client/pipeline.sh --no-gate
The branch is made if it does not exist. To use the worktree for another group later:
  git switch -c client/c4 main     (inside the worktree)
Remove it with: git worktree remove E:\SubVulpes-wt-a
#>
param(
    [Parameter(Mandatory = $true)][string]$Path,
    [Parameter(Mandatory = $true)][string]$Branch
)
$ErrorActionPreference = 'Stop'
$main = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
Set-Location $main

if (Test-Path $Path) { throw "$Path exists already" }
$exists = (git branch --list $Branch) -ne $null -and (git branch --list $Branch).Length -gt 0
if ($exists) { git worktree add $Path $Branch } else { git worktree add -b $Branch $Path main }
if ($LASTEXITCODE -ne 0) { throw 'git worktree add failed' }

function Link($from, $to) {
    $target = $to
    $item = Get-Item $to -Force
    if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { $target = $item.Target | Select-Object -First 1 }
    New-Item -ItemType Junction -Path $from -Target $target | Out-Null
}

# original/ has two versioned files (README.md, MANIFEST.sha256), so link its other entries one by one.
foreach ($entry in Get-ChildItem (Join-Path $main 'original') -Force) {
    $dest = Join-Path (Join-Path $Path 'original') $entry.Name
    if (Test-Path $dest) { continue }
    if ($entry.PSIsContainer) { Link $dest $entry.FullName } else { Copy-Item $entry.FullName $dest }
}
New-Item -ItemType Directory -Force -Path (Join-Path $Path 'build/lift'), (Join-Path $Path 'build/hook') | Out-Null
Link (Join-Path $Path 'build/recordings') (Join-Path $main 'build/recordings')
Copy-Item (Join-Path $main 'build/lift/client_win.jsonl') (Join-Path $Path 'build/lift/client_win.jsonl')
Copy-Item (Join-Path $main 'build/client_test') (Join-Path $Path 'build/client_test') -Recurse
Copy-Item (Join-Path $main 'build/client_deps') (Join-Path $Path 'build/client_deps') -Recurse
Copy-Item (Join-Path $main 'build/hook/subrosa_fixed.exe') (Join-Path $Path 'build/hook/subrosa_fixed.exe')
Write-Output "Worktree ready: $Path (branch $Branch)"
