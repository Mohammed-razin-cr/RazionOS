<#
Registers the Razion AI watchdog for the current Windows user.
No administrator privileges or stored password are required.
#>
param(
    [string]$TaskName = 'RazionOS AI Watchdog'
)

$ErrorActionPreference = 'Stop'
$watchdog = Join-Path $PSScriptRoot 'Watch-RazionAI.ps1'
if (-not (Test-Path -LiteralPath $watchdog)) {
    throw "Razion AI watchdog was not found: $watchdog"
}

$powerShell = (Get-Command powershell.exe).Source
$quotedWatchdog = '"' + $watchdog + '"'
$arguments = "-NoLogo -NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass -File $quotedWatchdog"
$action = New-ScheduledTaskAction -Execute $powerShell -Argument $arguments
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
$settings = New-ScheduledTaskSettingsSet `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit ([TimeSpan]::Zero) `
    -MultipleInstances IgnoreNew `
    -RestartCount 999 `
    -RestartInterval (New-TimeSpan -Minutes 1) `
    -StartWhenAvailable
$principal = New-ScheduledTaskPrincipal `
    -UserId ([Security.Principal.WindowsIdentity]::GetCurrent().Name) `
    -LogonType Interactive `
    -RunLevel Limited
$task = New-ScheduledTask -Action $action -Trigger $trigger -Settings $settings -Principal $principal

Register-ScheduledTask -TaskName $TaskName -InputObject $task -Force | Out-Null
Start-ScheduledTask -TaskName $TaskName

Write-Output "Enabled automatic Razion AI recovery for $env:USERNAME."
Write-Output "Scheduled task: $TaskName"
Write-Output "Watchdog: $watchdog"
