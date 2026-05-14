# build.ps1 — arduino-cli wrapper for the nulllab esp32-cam-v2 firmware.
#
# The V2 board is the classic ESP32 (LX6), pin-compatible with the
# AI-Thinker ESP32-CAM. The `esp32cam` preset in the arduino-esp32 core
# already configures PSRAM=Enabled, FlashSize=4M, etc.
#
# Examples:
#   .\build.ps1                                 # compile
#   .\build.ps1 -Strict                         # warnings=all
#   .\build.ps1 -Upload                         # compile + auto-detect + upload
#   .\build.ps1 -Upload -Port COM8              # compile + upload to specific port
#   .\build.ps1 -Upload -Monitor                # upload then attach serial monitor
#   .\build.ps1 -Network                        # OTA push via mDNS
#   .\build.ps1 -Network -NetHost camera-1      # OTA push to non-default hostname

[CmdletBinding()]
param(
    [string]$Port = "",
    [switch]$Upload,
    [switch]$Monitor,
    [switch]$MonitorOnly,    # skip compile/upload, just attach the serial monitor
    [switch]$Strict,
    [switch]$Network,
    [string]$NetHost = "esp32cam"
)

# MonitorOnly is the post-power-cycle escape hatch: the CH343P auto-reset is
# unreliable on this board, so a manual power-cycle is sometimes needed —
# which drops the previous monitor session. This re-attaches without rebuilding.
if ($MonitorOnly) {
    if (-not $Port) {
        $raw = arduino-cli board list --format json | ConvertFrom-Json
        $boards = if ($raw.detected_ports) { $raw.detected_ports } else { $raw }
        foreach ($b in $boards) {
            if ($b.port.protocol -eq "serial") { $Port = $b.port.address; break }
        }
    }
    if (-not $Port) { Write-Host "[build] No serial port found."; exit 1 }
    Write-Host "[build] Monitor-only on $Port at 115200. Ctrl+C to exit."
    # dtr=off,rts=off is critical on CH343P/CH340 boards — leaving either
    # asserted holds the ESP32 in reset and you see no output.
    arduino-cli monitor -p $Port -c baudrate=115200,dtr=off,rts=off
    return
}

# `esp32cam` preset baked-in: FlashSize=4M, PSRAM=enabled (quad), etc.
# We override PartitionScheme to min_spiffs so the app slot is ~1.9 MB
# (default would clip ESPAsyncWebServer + WiFiManager + camera). OTA
# stays available with this partition layout.
$Fqbn = "esp32:esp32:esp32cam:PartitionScheme=min_spiffs"

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
    # -fpermissive demotes one error to a warning: ESP_Async_WebServer 3.11.0
    # has `tcp_state state() const` that calls `_server.status()` where status()
    # is non-const in AsyncTCP. The cast is harmless; the library will catch up.
    "--build-property"; "compiler.cpp.extra_flags=-fpermissive"
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
    Write-Host "[build] Auto-detecting ESP32 port..."
    $raw = arduino-cli board list --format json | ConvertFrom-Json
    $boards = if ($raw.detected_ports) { $raw.detected_ports } else { $raw }

    $candidates = @()
    foreach ($b in $boards) {
        if ($b.port.protocol -ne "serial") { continue }
        $fqbns = @()
        if ($b.matching_boards) { $fqbns = @($b.matching_boards.fqbn) }

        # CH343P shows up as a generic USB-serial device, so arduino-cli often
        # can't match it to a specific board. We score 1 (any esp32:*) plus
        # an explicit catch-all for unmatched serial ports.
        $score = 0
        if ($fqbns | Where-Object { $_ -like "esp32:*" }) { $score = 1 }
        if ($fqbns.Count -eq 0)                            { $score = 1 }  # unmatched but serial → consider it
        if ($score -gt 0) {
            $candidates += [pscustomobject]@{
                Port = $b.port.address; Score = $score; Fqbns = $fqbns
            }
        }
    }
    if (-not $candidates) {
        Write-Host "[build] No serial port detected."
        Write-Host "[build] Plug in the board (USB-C) or pass -Port COMx explicitly."
        Write-Host "[build] Note: V2 uses a CH343P bridge; install the driver if Windows doesn't find one."
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
    # dtr=off,rts=off is critical on CH343P/CH340 boards — leaving either
    # asserted holds the ESP32 in reset and you see no output.
    arduino-cli monitor -p $Port -c baudrate=115200,dtr=off,rts=off
}
