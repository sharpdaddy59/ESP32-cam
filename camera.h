// camera.h — esp_camera wrapper for the nulllaborg ESP32-S3-CAM.

#pragma once

#include <Arduino.h>
#include <esp_camera.h>

// Initialize the camera with sensible defaults (UXGA frame size when PSRAM
// is available, JPEG quality 10). Returns true on success. Camera frame
// buffers are allocated in PSRAM — call this EARLY in setup() before WiFi
// and the web server fragment the heap.
bool camera_start();

// Free all camera resources (called before MSC mode if needed for memory).
// camera_start() can be called again afterwards.
void camera_stop();

bool camera_running();

// Capture a single frame. Caller must release with esp_camera_fb_return().
// Returns nullptr on failure or if the camera isn't running.
camera_fb_t *camera_grab();

// Apply runtime settings from a JSON-like struct of overrides. All fields
// are optional; pass -1 to leave a setting unchanged. See http_server.cpp
// for the JSON shape the UI sends.
struct CameraSettings {
  int framesize  = -1;   // framesize_t enum
  int quality    = -1;   // 0..63, lower = better
  int brightness = -1;   // -2..2
  int contrast   = -1;   // -2..2
  int saturation = -1;   // -2..2
  int hmirror    = -1;   // 0 / 1
  int vflip      = -1;   // 0 / 1
  int wb_mode    = -1;   // 0..4
  int ae_level   = -1;   // -2..2
  int agc_gain   = -1;   // 0..30
};
bool camera_apply_settings(const CameraSettings &s);

// Read current settings (whatever the sensor reports right now). Useful
// for the GET /camera endpoint and after-update echoes.
CameraSettings camera_get_settings();

// Flash LED PWM (both LEDs are on the same GPIO3, so this drives both).
// duty is 0..255.
void flash_led_init();
void flash_led_set(uint8_t duty);
uint8_t flash_led_get();
