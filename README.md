# ESP32-CAM

3D-printable enclosure and Arduino firmware for the [nulllaborg
esp32-cam-v2](https://github.com/nulllaborg/esp32-cam-v2) board (classic
ESP32, AI-Thinker-compatible pinout, OV2640 or OV3660 sensor).

## Layout

```
.
├── ESP32-CAM.ino       firmware entry point (boot order, main loop)
├── camera.cpp/.h       esp_camera config + flash-LED PWM
├── http_server.cpp/.h  AsyncWebServer routes + embedded HTML UI
├── net.cpp/.h          WiFi (via WiFiManager), mDNS, ArduinoOTA, NTP
├── sd.cpp/.h           optional SD_MMC + snapshot writer
├── device_name.cpp/.h  per-MAC mDNS hostname + NVS override
├── ota.cpp/.h          /ota and /ota/upload routes
├── config.h            pin defs, version, timing tunables
├── build.ps1           arduino-cli wrapper (compile, upload, OTA, monitor)
├── setup.ps1           one-time dependency install
└── enclosure/          OpenSCAD enclosure — see enclosure/README.md
```

## The enclosure

Two-piece friction-fit case in OpenSCAD. The four M2 mounting posts in the
back shell double as the case-closure mechanism — hollow tubes in the lid
slide over the post shafts. No screws, no snap hooks, no metal inserts.

Print settings, friction-fit tuning, and feature toggles live in
[`enclosure/README.md`](enclosure/README.md).

## The firmware

### What it does

- Embedded single-page web UI (no external assets, no filesystem needed)
- Live MJPEG `/stream`
- `/snapshot` (saves to SD if a card is present)
- Camera-settings sliders: resolution, JPEG quality, brightness, contrast,
  saturation, AE level, AGC gain, white-balance mode, hmirror, vflip
- Flash-LED PWM slider
- `/status` JSON: heap, PSRAM, RSSI, IP, hostname, SD info, CPU temp
- mDNS (`http://<hostname>.local/`)
- OTA over Wi-Fi via ArduinoOTA
- WiFiManager captive portal for first-time provisioning
- SD card is optional and absent-card-safe

### Setup (one-time)

```
.\setup.ps1
```

Installs `arduino-cli` (via winget if missing), the `esp32:esp32` core, and
the required libraries: `ArduinoJson`, `ESP Async WebServer` (ESP32Async
fork), `WiFiManager`, and `ESP32Async/AsyncTCP` (the latter from a git URL
because the Library Manager `AsyncTCP` listing is an older fork that
crashes on arduino-esp32 3.x with a lwIP TCPIP core-locking assertion).

### Build / flash / monitor

```
.\build.ps1                     compile
.\build.ps1 -Upload -Monitor    compile + flash + open serial monitor
.\build.ps1 -Network            OTA push via mDNS (no cable)
.\build.ps1 -MonitorOnly        re-attach monitor after a power cycle
```

FQBN: `esp32:esp32:esp32cam:PartitionScheme=min_spiffs`. App slot is
~1.9 MB. Current binary is ~1.4 MB (71% used) with headroom for growth and
the OTA sister-partition.

### First-time Wi-Fi setup

A fresh board with no saved credentials opens an open AP named
`esp32cam-setup-XXXX` (`XXXX` is the last 4 hex chars of the MAC).
Connect with your phone — most devices launch a captive-portal browser;
if not, visit `http://192.168.4.1`. Pick your home Wi-Fi, enter the
password, submit. The board persists credentials to NVS and connects.

**Press RESET on the board after first provisioning.** See "Gotchas"
below. Subsequent boots skip the portal entirely.

Then open `http://esp32cam-XXXX.local/` (or the IP from the serial log).

Use `POST /wifi/reset` (or the "Forget Wi-Fi creds" button in the UI) to
wipe credentials and re-open the portal.

## Gotchas

Documented in the code, but worth surfacing here:

- **CH343P auto-reset is flaky.** After `arduino-cli upload` says
  "Hard resetting via RTS pin...", press the physical RESET button. The
  monitor session survives a RESET press; it does not survive a power
  cycle.
- **Serial monitor needs `dtr=off,rts=off`** or RTS holds the chip in
  reset and you see no output. `build.ps1 -Monitor` and `-MonitorOnly`
  set this automatically.
- **First-boot port 80 conflict.** WiFiManager runs its own web server
  on port 80 while the portal is open. When it exits and our
  AsyncWebServer tries to bind the same port, the handoff isn't always
  clean and our HTTP server may silently fail to listen. Pressing RESET
  after successful provisioning works around this; subsequent boots are
  fine because they skip the portal.
- **Snapshots compete with the live stream for Wi-Fi bandwidth.** Both
  share one radio link. For fast snapshots, close the home page first or
  drop the stream resolution to SVGA in the camera settings.
- **Builds need `-fpermissive`.** ESPAsyncWebServer 3.11.0's
  `AsyncWebServer::state() const` calls a non-const method on its
  underlying `AsyncServer`. Harmless cast; `-fpermissive` demotes the
  error to a warning. `build.ps1` passes it automatically.

## Roadmap

- Fix the port-80 handoff so first-boot provisioning doesn't need a
  manual reset.
- Stream rate-limit / framerate selector in the UI.
- Snapshot endpoint variant with quality decoupled from the live stream.
- Motion detection via a PSRAM-resident pre-trigger ring buffer.
- mDNS service-browsing page to discover other cameras on the LAN.
