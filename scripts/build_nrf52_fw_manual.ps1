param(
    [string]$EnvName = "nrf52840_pca10059"
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path -Parent $PSScriptRoot
$firmwareRoot = Join-Path $workspaceRoot "esp32-ble-connection-manager"
$firmwareScript = Join-Path $firmwareRoot "build_nrf52_fw_manual.ps1"

if (!(Test-Path $firmwareScript)) {
    throw "Firmware build script not found: $firmwareScript"
}

Write-Host "Workspace wrapper: building nRF52 firmware from nested project" -ForegroundColor Cyan
Write-Host "Firmware project: $firmwareRoot" -ForegroundColor Cyan

& pwsh -NoProfile -ExecutionPolicy Bypass -File $firmwareScript -EnvName $EnvName
if ($LASTEXITCODE -ne 0) {
    throw "Nested firmware build script failed with exit code $LASTEXITCODE"
}
