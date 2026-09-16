<#
Keeps the host-side llama.cpp service available for a RazionOS VM.
The watchdog is intended to run as a per-user Windows scheduled task.
#>
param(
    [ValidateRange(5, 300)][int]$CheckIntervalSeconds = 15,
    [ValidateRange(1, 65535)][int]$Port = 8080
)

$ErrorActionPreference = 'Continue'
$endpoint = "http://127.0.0.1:$Port/health"
$startScript = Join-Path $PSScriptRoot 'Start-RazionAI.ps1'
$logDirectory = Join-Path $env:LOCALAPPDATA 'RazionOS\AI\logs'
$watchdogLog = Join-Path $logDirectory 'watchdog.log'
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null

if (-not (Test-Path -LiteralPath $startScript)) {
    throw "Razion AI start script was not found: $startScript"
}

$createdNew = $false
$mutex = [Threading.Mutex]::new($true, 'Local\RazionOSAIWatchdog', [ref]$createdNew)
if (-not $createdNew) {
    exit 0
}

function Write-WatchdogLog {
    param([string]$Message)
    Add-Content -LiteralPath $watchdogLog -Value "$(Get-Date -Format o) $Message"
}

function Test-RazionAIHealth {
    try {
        $health = Invoke-RestMethod -Uri $endpoint -TimeoutSec 3
        return $health.status -eq 'ok'
    } catch {
        return $false
    }
}

try {
    Write-WatchdogLog 'Watchdog started.'
    while ($true) {
        if (-not (Test-RazionAIHealth)) {
            Write-WatchdogLog 'AI service unavailable; starting llama.cpp.'
            try {
                & $startScript -Port $Port -ReadyTimeoutSeconds 300 |
                    ForEach-Object { Write-WatchdogLog $_ }
            } catch {
                Write-WatchdogLog "Restart failed: $($_.Exception.Message)"
            }
        }
        Start-Sleep -Seconds $CheckIntervalSeconds
    }
} finally {
    Write-WatchdogLog 'Watchdog stopped.'
    $mutex.ReleaseMutex()
    $mutex.Dispose()
}
