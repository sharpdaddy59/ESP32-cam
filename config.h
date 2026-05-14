// config.h — central tunables for the nulllab esp32-cam-v2 firmware.
//
// The V2 board is built around the classic ESP32-D0WD-V3 (LX6, not S3).
// It is pin-compatible with the AI-Thinker ESP32-CAM — same camera pins,
// same SD-MMC pins, same flash-LED pin. We use the bundled
// `camera_pins.h` from the esp32-camera library with
// CAMERA_MODEL_AI_THINKER defined; the pin numbers below are for
// cross-reference only.

#pragma once

// -- Identity ---------------------------------------------------------------
#define FW_VERSION       "0.3.0"
#define MDNS_HOSTNAME    "esp32cam"

// -- HTTP -------------------------------------------------------------------
#define HTTP_PORT                80

// -- WiFi reconnection backoff ---------------------------------------------
#define WIFI_BACKOFF_1_MS     15000
#define WIFI_BACKOFF_2_MS     30000
#define WIFI_BACKOFF_3_MS     60000
#define WIFI_BACKOFF_4_MS    120000
#define WIFI_BACKOFF_5_MS    240000
#define WIFI_BACKOFF_CAP_MS  300000

// -- WiFiManager (captive portal) ------------------------------------------
// AP name shown to phone/laptop when the portal is up. Suffixed with the
// last 4 hex chars of the WiFi STA MAC at runtime (see net.cpp).
#define WM_AP_PREFIX          "esp32cam-setup"
// Time the portal stays open with no client before giving up and rebooting.
// Long enough for the user to fish out their phone, short enough that an
// unattended board doesn't sit advertising an open AP forever.
#define WM_PORTAL_TIMEOUT_S   300

// -- Camera pinout — AI-Thinker layout (esp32-cam-v2) ----------------------
// These are duplicated by `camera_pins.h` when CAMERA_MODEL_AI_THINKER is
// defined; we keep them here only for documentation. camera.cpp uses the
// header's defines, not these.
//
//   PWDN   GPIO32        SIOC   GPIO27        VSYNC  GPIO25
//   RESET  -1            D7     GPIO35        HREF   GPIO23
//   XCLK   GPIO0         D6     GPIO34        PCLK   GPIO22
//   SIOD   GPIO26        D5     GPIO39
//                        D4     GPIO36
//                        D3     GPIO21
//                        D2     GPIO19
//                        D1     GPIO18
//                        D0     GPIO5

// -- SD_MMC pinout (AI-Thinker, fixed in hardware) -------------------------
// 1-BIT MODE ONLY. Using 4-bit mode wires SD's D1 onto GPIO4 — which is also
// the flash-LED pin, so every SD access would flicker the LED. The SD_MMC
// driver's 1-bit mode claims only CLK / CMD / D0 (14 / 15 / 2) and leaves
// GPIO4 alone. Treat this as load-bearing — do not switch to 4-bit.
#define SD_PIN_CLK       14
#define SD_PIN_CMD       15
#define SD_PIN_D0         2

// -- Flash LED (single LED with driver transistor) -------------------------
#define FLASH_LED_PIN     4
#define FLASH_LED_CHANNEL 7      // ledc channel — keep clear of camera's XCLK (uses channel 0)
#define FLASH_LED_FREQ    5000   // Hz
#define FLASH_LED_RES_BITS 8     // 0..255 duty

// (No software-controllable status LED on this board. The red "Power_Red"
// LED visible on the PCB is D2 — hardwired across the 3V3 rail through R8.
// Only physical mitigation: tape, paint, or desolder R8 to dim/kill it.)

// -- SD layout -------------------------------------------------------------
#define SD_PATH_SNAPSHOTS   "/snapshots"
