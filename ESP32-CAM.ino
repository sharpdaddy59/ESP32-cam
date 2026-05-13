// ESP32-CAM.ino — boot for the nulllaborg ESP32-S3-CAM firmware.
//
// Build:    .\build.ps1                          (compile)
//           .\build.ps1 -Upload -Monitor         (compile + flash + serial)
//           .\build.ps1 -Network                 (OTA push via mDNS)
// Setup:    .\setup.ps1                          (one-time, installs deps)
//
// Boot order:
//   1. Serial via USB CDC
//   2. SD_MMC mount  (creds + snapshot dir live here)
//   3. camera_start  (PSRAM frame buffers — claim them BEFORE WiFi/HTTP)
//   4. flash_led_init
//   5. device_name_init (per-MAC default hostname; uses efuse MAC, no WiFi needed)
//   6. net_begin     (reads creds from SD, kicks off WiFi connect)
//   7. http_server_begin (AsyncWebServer; binds even if WiFi isn't up yet)
//   8. usb_msc_init  (USB peripheral up; presents SD when host enumerates)

#include <Arduino.h>

#include "config.h"
#include "sd.h"
#include "camera.h"
#include "device_name.h"
#include "net.h"
#include "http_server.h"
#include "usb_msc.h"

void setup() {
  Serial.begin(115200);
  // Brief grace for USB CDC enumeration before the first log lines.
  delay(200);
  Serial.println();
  Serial.println("[boot] esp32s3-cam " FW_VERSION);

  if (!sd_mount()) {
    Serial.println("[boot] WARNING: SD mount failed — wifi.json will be unreadable,");
    Serial.println("[boot] USB MSC will be unavailable. Insert a card and reboot.");
  }

  if (!camera_start()) {
    Serial.println("[boot] WARNING: camera init failed — /stream and /snapshot will 500.");
    Serial.println("[boot] Verify pin definitions in config.h against the schematic.");
  }

  flash_led_init();

  device_name_init();

  net_begin();

  http_server_begin();

  usb_msc_init();

  Serial.println("[boot] setup complete");
}

void loop() {
  net_loop();
  delay(10);
}
