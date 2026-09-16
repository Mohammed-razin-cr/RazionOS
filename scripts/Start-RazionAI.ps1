<#
Starts a local llama.cpp model server for a VirtualBox RazionOS guest.
The server listens only on Windows loopback. RazionOS reaches it through
VirtualBox NAT localhost-reachability at guest address 10.0.2.2.
#>
param(
    [string]$ModelRepo = 'ggml-org/Qwen3.5-0.8B-GGUF',
    [string]$ModelFile = 'Qwen3.5-0.8B-Q4_0.gguf',
    [ValidateRange(1024, 32768)][int]$ContextSize = 2048,
    [ValidateRange(1, 65535)][int]$Port = 8080,
    [ValidateRange(5, 300)][int]$ReadyTimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$endpoint = "http://127.0.0.1:$Port/v1/models"

function Test-RazionModelReady {
    param([string]$Uri)
    try {
        $response = Invoke-RestMethod -Uri $Uri -TimeoutSec 3
        return ($null -ne $response.data -and $response.data.Count -gt 0 -and
            -not [string]::IsNullOrWhiteSpace($response.data[0].id))
    } catch {
        return $false
    }
}

if (Test-RazionModelReady -Uri $endpoint) {
    Write-Output "Razion AI model server is already ready at $endpoint"
    exit 0
}

$listener = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue |
    Where-Object { $_.LocalAddress -eq '127.0.0.1' -or $_.LocalAddress -eq '0.0.0.0' } |
    Select-Object -First 1
if ($listener) {
    throw "TCP $Port is already in use (PID $($listener.OwningProcess)); wait for that model to load or free the port before retrying."
}

$command = Get-Command llama-server -ErrorAction SilentlyContinue
if (-not $command) {
    $env:PATH = [Environment]::GetEnvironmentVariable('PATH', 'User') + ';' +
        [Environment]::GetEnvironmentVariable('PATH', 'Machine')
    $command = Get-Command llama-server -ErrorAction SilentlyContinue
}
if (-not $command) {
    throw 'llama-server was not found. Install ggml.llamacpp or add llama-server to PATH.'
}

$arguments = @(
    '-hf', $ModelRepo,
    '-hff', $ModelFile,
    '--host', '127.0.0.1',
    '--port', "$Port",
    '--ctx-size', "$ContextSize"
)
$server = Start-Process -FilePath $command.Source -ArgumentList $arguments -WindowStyle Hidden -PassThru
Write-Output "Starting llama.cpp on host loopback (PID $($server.Id))."

for ($elapsed = 0; $elapsed -lt $ReadyTimeoutSeconds; $elapsed += 2) {
    if (Test-RazionModelReady -Uri $endpoint) {
        Write-Output "Razion AI model server is ready at $endpoint"
        Write-Output 'In RazionOS, run: razion-ai-status providers; razion-chat'
        exit 0
    }
    if ($server.HasExited) {
        throw "llama-server exited with code $($server.ExitCode) before the model became ready."
    }
    Start-Sleep -Seconds 2
    $server.Refresh()
}

throw "llama-server is still loading after $ReadyTimeoutSeconds seconds (PID $($server.Id)). Check $endpoint shortly; the first model download can take longer."
