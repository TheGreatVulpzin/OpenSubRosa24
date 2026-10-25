# Run the GhidraMCP server as its own process, with the HTTP transport.
#
# With stdio, the server is a child of the Claude session, and only /mcp can start it again.
# With HTTP, this script stops and starts it, and the session connects to the URL
# (.mcp.json: "type": "http", "url": "http://127.0.0.1:8081/mcp").
#
# The server opens a copy of the Ghidra project with --discard (the headless scripts need
# the real project, and a project has one lock). Changes made through the MCP server are
# lost. Changes that must stay go into a script or a CSV, then into the real project.
#
# Usage: powershell -File instrumentation\ghidra\mcp_server.ps1 start|stop|restart|refresh|status
#   refresh: stop, copy the real project again (after ApplyTypes, ApplyNames...), start.
param([Parameter(Mandatory = $true)][ValidateSet('start', 'stop', 'restart', 'refresh', 'status')][string]$Action)

$ErrorActionPreference = 'Stop'
$Root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$Exe = if ($env:GHIDRA_MCP_EXE) { $env:GHIDRA_MCP_EXE } else { 'E:\tools\ghidramcp-venv\Scripts\ghidra-mcp.exe' }
$Copy = if ($env:GHIDRA_MCP_HTTP_PROJECT) { $env:GHIDRA_MCP_HTTP_PROJECT } else { 'E:\subvulpes\ghidra-mcp-http' }
$Port = if ($env:GHIDRA_MCP_PORT) { $env:GHIDRA_MCP_PORT } else { '8081' }
$Log = Join-Path $Root 'build\ghidra-mcp.log'
if (-not $env:JAVA_HOME) { $env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.7.6-hotspot' }
if (-not $env:GHIDRA_INSTALL_DIR) { $env:GHIDRA_INSTALL_DIR = 'E:\tools\ghidra_12.1.4_PUBLIC' }

# Only the HTTP server of this script: a stdio server of a session has no --mcp-port.
function Get-Server {
    Get-CimInstance Win32_Process -Filter "Name='ghidra-mcp.exe'" |
        Where-Object { $_.CommandLine -match "--mcp-port\s+$Port" }
}

function Stop-Server {
    foreach ($p in Get-Server) {
        Stop-Process -Id $p.ProcessId -Force -Confirm:$false
        Write-Output "stopped $($p.ProcessId)"
    }
    # The JVM releases the project lock a moment after the process ends.
    Start-Sleep -Seconds 2
}

function Copy-Project {
    $src = Join-Path $Root 'ghidra-project'
    if (Test-Path $Copy) { Remove-Item -Recurse -Force $Copy }
    New-Item -ItemType Directory -Force $Copy | Out-Null
    Copy-Item -Recurse (Join-Path $src 'subvulpes.gpr'), (Join-Path $src 'subvulpes.rep') $Copy
    Get-ChildItem $Copy -Filter '*.lock*' | Remove-Item -Force
    Write-Output "copied $src -> $Copy"
}

function Start-Server {
    if (Get-Server) { Write-Output 'already running'; return }
    if (-not (Test-Path $Copy)) { Copy-Project }
    $mcpArgs = @('--transport', 'streamable-http', '--mcp-port', $Port, '--headless',
        '--binary', 'original/blackhouser/subrosadedicated024d.exe', '--project', $Copy,
        '--project-name', 'subvulpes', '--no-analyze', '--discard')
    $p = Start-Process -FilePath $Exe -ArgumentList $mcpArgs -WorkingDirectory $Root -WindowStyle Hidden `
        -RedirectStandardOutput $Log -RedirectStandardError "$Log.err" -PassThru
    Write-Output "started $($p.Id), http://127.0.0.1:$Port/mcp, log $Log"
}

switch ($Action) {
    'start' { Start-Server }
    'stop' { Stop-Server }
    'restart' { Stop-Server; Start-Server }
    'refresh' { Stop-Server; Copy-Project; Start-Server }
    'status' { $s = Get-Server; if ($s) { $s | Select-Object ProcessId, CommandLine } else { 'not running' } }
}
