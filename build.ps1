# build.ps1 — arduino-cli wrapper for the ESP32-S3-CAM firmware.
#
# Uses the mainstream esp32:esp32:esp32s3 core (not the M5Stack fork that
# cores3-hydro uses). The nulllaborg board is a generic ESP32-S3 dev board
# — the default core handles it correctly.
#
# Examples:
#   .\build.ps1                                 # compile (default)
#   .\build.ps1 -Strict                         # warnings=all
#   .\build.ps1 -Upload                         # compile + auto-detect + upload
#   .\build.ps1 -Upload -Port COM8              # compile + upload to specific port
#   .\build.ps1 -Upload -Monitor                # upload then attach serial monitor
#   .\build.ps1 -Network                        # OTA push via mDNS
#   .\build.ps1 -Network -NetHost camera-1      # OTA push to a non-default hostname
#
# Pre-reqs (run setup.ps1 once):
#   - arduino-cli on PATH
#   - esp32:esp32 core installed
#   - ESPAsyncWebServer, AsyncTCP, ArduinoJson installed

[CmdletBinding()]
param(
    [string]$Port = "",
    [switch]$Upload,
    [switch]$Monitor,
    [switch]$Strict,
    [switch]$Network,
    [string]$NetHost = "esp32s3-cam"
)

# FQBN with board-config options. Critical settings:
#   PSRAM=opi        : nulllab board has WROOM-1 N16R8 with octal-SPI PSRAM
#   CDCOnBoot=cdc    : serial-over-USB on the native USB-C (no FTDI on this board)
#   USBMode=hwcdc    : hardware CDC, required for the USBMSC TinyUSB stack
#   FlashSize=8M     : adjust to 16M if your board is the 16MB variant
#   PartitionScheme=default_8MB : OTA + SPIFFS, fits in 8MB
$Fqbn = "esp32:esp32:esp32s3:PSRAM=opi,CDCOnBoot=cdc,USBMode=hwcdc,FlashSize=8M,PartitionScheme=default_8MB"

function Find-SketchDir {
    foreach ($c in @($PWD.Path, $PSScriptRoot) | Select-Object -Unique) {
        $leaf = Split-Path -Leaf $c
        if (Test-Path (Join-Path $c "$leaf.ino")) { return $c }
    }
    return $null
}
$SketchDir = Find-SketchDir
if (-not $SketchDir) {
    Write-Host "[build] No matching .ino found. arduino-cli requires <dir>/<dir>.ino."
    Write-Host "[build] Expected ESP32-CAM.ino inside a folder named ESP32-CAM."
    exit 1
}

$warn = if ($Strict) { "all" } else { "default" }
Write-Host "[build] Compiling (FQBN: $Fqbn, warnings=$warn)"

$compileArgs = @(
    "compile"
    "--fqbn"; $Fqbn
    "--warnings"; $warn
    "--export-binaries"
    $SketchDir
)
& arduino-cli @compileArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$binDir = Join-Path $SketchDir "build\$($Fqbn -replace ':', '.' -replace ',', '.')"
$binPath = Join-Path $binDir "$(Split-Path -Leaf $SketchDir).ino.bin"
if (Test-Path $binPath) {
    Write-Host "[build] Firmware .bin: $binPath"
}

if ($Network) {
    $netTarget = "$NetHost.local"
    Write-Host "[build] Pushing OTA to $netTarget (ArduinoOTA / port 3232)"
    arduino-cli upload --protocol network --port $netTarget --fqbn $Fqbn $SketchDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "[build] OTA push complete."
    return
}

if (-not $Upload) {
    Write-Host "[build] Compile OK."
    return
}

if (-not $Port) {
    Write-Host "[build] Auto-detecting ESP32-S3 port..."
    $raw = arduino-cli board list --format json | ConvertFrom-Json
    $boards = if ($raw.detected_ports) { $raw.detected_ports } else { $raw }

    $candidates = @()
    foreach ($b in $boards) {
        if ($b.port.protocol -ne "serial") { continue }
        $fqbns = @()
        if ($b.matching_boards) { $fqbns = @($b.matching_boards.fqbn) }

        $score = 0
        if ($fqbns | Where-Object { $_ -like "esp32:esp32:esp32s3*" }) { $score = 3 }
        elseif ($fqbns | Where-Object { $_ -like "esp32:*" })          { $score = 1 }
        if ($score -gt 0) {
            $candidates += [pscustomobject]@{
                Port = $b.port.address; Score = $score; Fqbns = $fqbns
            }
        }
    }
    if (-not $candidates) {
        Write-Host "[build] No ESP32-family serial port detected."
        Write-Host "[build] Plug in the board (USB-C) or pass -Port COMx explicitly."
        exit 1
    }
    $best = $candidates | Sort-Object -Property Score -Descending | Select-Object -First 1
    $Port = $best.Port
    Write-Host "[build] Using port $Port"
}

Write-Host "[build] Uploading to $Port..."
arduino-cli upload -p $Port --fqbn $Fqbn $SketchDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Monitor) {
    Write-Host "[build] Opening serial monitor on $Port at 115200. Ctrl+C to exit."
    arduino-cli monitor -p $Port -c baudrate=115200
}
