// config.h — central tunables for the nulllaborg ESP32-S3-CAM firmware.
//
// Everything you might want to adjust without hunting through the rest of
// the codebase lives here. Pin assignments, intervals, timeouts, version.

#pragma once

// -- Identity ---------------------------------------------------------------
#define FW_VERSION       "0.1.0"
// MDNS_HOSTNAME is the *base* default — actual runtime hostname comes from
// device_hostname() in device_name.h, which appends a per-MAC suffix on
// first boot and supports user override (NVS, /hostname endpoint).
#define MDNS_HOSTNAME    "esp32s3-cam"

// -- HTTP -------------------------------------------------------------------
#define HTTP_PORT                80

// -- WiFi reconnection backoff ---------------------------------------------
#define WIFI_BACKOFF_1_MS     15000
#define WIFI_BACKOFF_2_MS     30000
#define WIFI_BACKOFF_3_MS     60000
#define WIFI_BACKOFF_4_MS    120000
#define WIFI_BACKOFF_5_MS    240000
#define WIFI_BACKOFF_CAP_MS  300000

// -- USB Mass Storage decide window ----------------------------------------
// On boot we wait this long for a USB host to enumerate before committing to
// standalone mode. If enumeration happens after this, MSC switches on
// live via the TinyUSB event callback.
#define MSC_DECIDE_MS        1000

// -- Camera pinout — nulllaborg ESP32-S3-CAM -------------------------------
// IMPORTANT: VERIFY against esp32s3_cam_sch.pdf in the nulllaborg repo.
// These defaults match the common Freenove/generic ESP32-S3-CAM layout
// that the nulllab board is most likely to share. If the camera fails to
// init with "Camera probe failed", these pins are the first thing to check.
#define CAM_PIN_PWDN     -1
#define CAM_PIN_RESET    -1
#define CAM_PIN_XCLK     15
#define CAM_PIN_SIOD      4   // I2C SDA
#define CAM_PIN_SIOC      5   // I2C SCL
#define CAM_PIN_D7       16
#define CAM_PIN_D6       17
#define CAM_PIN_D5       18
#define CAM_PIN_D4       12
#define CAM_PIN_D3       10
#define CAM_PIN_D2        8
#define CAM_PIN_D1        9
#define CAM_PIN_D0       11
#define CAM_PIN_VSYNC     6
#define CAM_PIN_HREF      7
#define CAM_PIN_PCLK     13

// -- SD_MMC pinout ---------------------------------------------------------
// 1-bit mode (the most common arrangement on tight S3-CAM boards). VERIFY
// against the schematic — some variants put SD on the JTAG pins.
#define SD_PIN_CLK       39
#define SD_PIN_CMD       38
#define SD_PIN_D0        40

// -- Flash LED (two LEDs in parallel on GPIO3) -----------------------------
#define FLASH_LED_PIN     3
#define FLASH_LED_CHANNEL 0      // ledc channel
#define FLASH_LED_FREQ    5000   // Hz
#define FLASH_LED_RES_BITS 8     // 0..255 duty

// -- SD layout -------------------------------------------------------------
#define SD_PATH_WIFI_JSON   "/config/wifi.json"
#define SD_PATH_SNAPSHOTS   "/snapshots"

// -- USB device strings ----------------------------------------------------
#define USB_MANUFACTURER  "nulllaborg"
#define USB_PRODUCT       "ESP32-S3-CAM"
