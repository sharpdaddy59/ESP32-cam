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

#include <Preferences.h>

#include "config.h"
#include "sd.h"
#include "camera.h"
#include "device_name.h"
#include "net.h"
#include "http_server.h"
#include "motion.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("[boot] esp32-cam-v2 " FW_VERSION);

  // SD is optional. A missing or unformatted card logs once and we move on.
  sd_mount();

  // Initial camera config depends on the saved mode in NVS. Default is
  // STREAM (current behavior); if the device was last in MOTION mode we
  // boot straight into grayscale config and skip the JPEG-mode warm-up.
  Preferences mode_prefs;
  int saved_mode = MODE_STREAM;
  if (mode_prefs.begin("mode", /*readOnly=*/true)) {
    saved_mode = mode_prefs.getInt("m", MODE_STREAM);
    mode_prefs.end();
  }
  bool cam_ok = (saved_mode == MODE_MOTION) ? camera_start_motion() : camera_start();
  if (!cam_ok) {
    Serial.println("[boot] WARNING: camera init failed — /stream and /snapshot will 500.");
    Serial.println("[boot] Verify the FFC is fully seated and the lens isn't covered.");
  }
  // Overlay any user-tuned camera settings persisted to NVS by previous
  // /camera POSTs. No-op on first boot or after /camera/reset. Settings
  // like brightness/contrast apply to both JPEG and GRAYSCALE modes.
  camera_load_and_apply_settings();

  flash_led_init();
  // Restore last saved flash-LED duty. The LED comes on at the last
  // committed value at boot — set the slider to 0 and Apply if you want
  // it dark by default.
  flash_led_load();

  device_name_init();

  // BLOCKS: opens the WiFiManager captive portal if no creds are stored.
  // After this returns, WiFi is up (or the device has rebooted).
  net_begin();

  http_server_begin();

  // Motion module: spawns its FreeRTOS task; the task gates on mode and
  // is a cheap no-op when not in MOTION mode.
  motion_init();

  Serial.println("[boot] setup complete");
}

void loop() {
  net_loop();
  delay(10);
}
