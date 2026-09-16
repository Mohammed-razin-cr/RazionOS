<#
Installs the official llama.cpp Windows CPU runtime for RazionOS host-side AI.
The runtime and model cache stay outside the repository and bootable ISO.
#>
param(
    [string]$BuildTag = '',
    [switch]$Start
)

$ErrorActionPreference = 'Stop'
$headers = @{ 'User-Agent' = 'RazionOS-AI-Installer' }

if ([string]::IsNullOrWhiteSpace($BuildTag)) {
    $pointer = Invoke-WebRequest -UseBasicParsing `
        -Uri 'https://github.com/ggml-org/llama.cpp/releases/download/v0.4.1/nightly-tag.txt'
    if ($pointer.Content -is [byte[]]) {
        $BuildTag = [Text.Encoding]::UTF8.GetString($pointer.Content).Trim()
    } else {
        $BuildTag = ([string]$pointer.Content).Trim()
    }
}
if ($BuildTag -notmatch '^b[0-9]+$') {
    throw "Unexpected llama.cpp build tag: $BuildTag"
}

$release = Invoke-RestMethod `
    -Uri "https://api.github.com/repos/ggml-org/llama.cpp/releases/tags/$BuildTag" `
    -Headers $headers
$assetName = "llama-$BuildTag-bin-win-cpu-x64.zip"
$asset = $release.assets | Where-Object { $_.name -eq $assetName } | Select-Object -First 1
if (-not $asset) {
    throw "The official Windows CPU package $assetName was not found."
}

$installDirectory = Join-Path $env:LOCALAPPDATA 'RazionOS\AI\llama.cpp'
$archive = Join-Path $env:TEMP $assetName
New-Item -ItemType Directory -Path $installDirectory -Force | Out-Null
Write-Output "Downloading official llama.cpp $BuildTag CPU runtime..."
Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $archive
Expand-Archive -LiteralPath $archive -DestinationPath $installDirectory -Force

$server = Join-Path $installDirectory 'llama-server.exe'
if (-not (Test-Path -LiteralPath $server)) {
    throw "Installation completed without llama-server.exe in $installDirectory"
}
Write-Output "Installed llama.cpp $BuildTag at $installDirectory"

if ($Start) {
    & (Join-Path $PSScriptRoot 'Start-RazionAI.ps1') -ServerPath $server -ReadyTimeoutSeconds 300
}
