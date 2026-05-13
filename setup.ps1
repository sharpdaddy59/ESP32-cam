# setup.ps1 — one-time arduino-cli configuration for the ESP32-S3-CAM.
#
# Idempotent — safe to re-run. Installs:
#   - arduino-cli itself (via winget) if missing
#   - The mainstream esp32:esp32 board package
#   - Required libraries: ESPAsyncWebServer, AsyncTCP, ArduinoJson
#
# The esp32-camera and TinyUSB integrations come bundled with the esp32 core.

$ErrorActionPreference = 'Stop'

$Sketchbook = 'C:\Users\gregl\Documents\Arduino'

function Have-Cmd($name) {
    $null -ne (Get-Command $name -ErrorAction SilentlyContinue)
}

if (-not (Have-Cmd 'arduino-cli')) {
    Write-Host '[setup] arduino-cli not found; installing via winget...'
    winget install --id ArduinoSA.CLI -e --accept-source-agreements --accept-package-agreements
    if (-not (Have-Cmd 'arduino-cli')) {
        Write-Host '[setup] winget installed arduino-cli, but it is not on PATH for this shell.'
        Write-Host '[setup] Open a NEW PowerShell window and re-run setup.ps1.'
        exit 1
    }
} else {
    Write-Host ('[setup] arduino-cli found: ' + ((arduino-cli version) -join ' '))
}

Write-Host '[setup] Initializing arduino-cli config (no-op if it already exists)...'
arduino-cli config init 2>$null | Out-Null

Write-Host "[setup] Pointing sketchbook at $Sketchbook"
arduino-cli config set directories.user "$Sketchbook"

Write-Host '[setup] Updating package index...'
arduino-cli core update-index

Write-Host '[setup] Installing esp32:esp32 core (mainstream arduino-esp32)...'
arduino-cli core install esp32:esp32

$libs = @(
    'ArduinoJson'
    'ESP Async WebServer'   # ESPAsyncWebServer; includes AsyncTCP as dep on ESP32
    'AsyncTCP'              # explicit in case the dep edge moves
)
foreach ($lib in $libs) {
    Write-Host "[setup] Installing library: $lib"
    arduino-cli lib install "$lib"
}

Write-Host ''
Write-Host '[setup] Verifying ESP32-S3 board is recognized...'
arduino-cli board listall esp32 | Select-String -Pattern 's3' -CaseSensitive:$false

Write-Host ''
Write-Host '[setup] Done. Try a build with:    .\build.ps1'
Write-Host '[setup] Plug in your board then:   .\build.ps1 -Upload -Monitor'
