// ESP32-CAM.ino — boot for the nulllab esp32-cam-v2 (classic ESP32).
//
// Hardware notes (vs. the original esp32s3-cam V1 firmware this was ported from):
//   - Classic ESP32 (no native USB; CH343P USB-serial bridge on the USB-C jack)
//   - AI-Thinker pinout for camera, SD, and flash LED
//   - 4 MB flash, 2 MB quad PSRAM
//   - No BOOT button (RESET only)
//   - No battery jack
//
// Build:    .\build.ps1                       (compile)
//           .\build.ps1 -Upload -Monitor      (compile + flash + serial)
//           .\build.ps1 -Network              (OTA push via mDNS)
// Setup:    .\setup.ps1                       (one-time, installs deps)
//
// Boot order:
//   1. Serial via the CH343P bridge (UART0 @ 115200)
//   2. SD_MMC mount  (optional — board works fine without a card)
//   3. camera_start  (PSRAM frame buffers claimed BEFORE WiFi/HTTP)
//   4. flash_led_init
//   5. device_name_init   (per-MAC default hostname; uses efuse MAC)
//   6. net_begin          (WiFiManager portal if no creds — BLOCKS until WiFi up)
//   7. http_server_begin  (must come AFTER net_begin — both want port 80)

#include <Arduino.h>

#include "config.h"
#include "sd.h"
#include "camera.h"
#include "device_name.h"
#include "net.h"
#include "http_server.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("[boot] esp32-cam-v2 " FW_VERSION);

  // SD is optional. A missing or unformatted card logs once and we move on.
  sd_mount();

  if (!camera_start()) {
    Serial.println("[boot] WARNING: camera init failed — /stream and /snapshot will 500.");
    Serial.println("[boot] Verify the FFC is fully seated and the lens isn't covered.");
  }

  flash_led_init();

  device_name_init();

  // BLOCKS: opens the WiFiManager captive portal if no creds are stored.
  // After this returns, WiFi is up (or the device has rebooted).
  net_begin();

  http_server_begin();

  Serial.println("[boot] setup complete");
}

void loop() {
  net_loop();
  delay(10);
}
