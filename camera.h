// camera.h — esp_camera wrapper for the nulllaborg ESP32-S3-CAM.

#pragma once

#include <Arduino.h>
#include <esp_camera.h>

// Initialize the camera for streaming/snapshots: JPEG @ UXGA (or SVGA
// without PSRAM). Returns true on success. Call EARLY in setup() before
// WiFi and the web server fragment the heap.
bool camera_start();

// Initialize the camera for motion detection: GRAYSCALE @ QVGA. Switching
// between this and camera_start() requires a camera_stop() in between —
// the esp_camera library doesn't support a clean in-place pixel-format
// change. Motion mode uses smaller buffers (~76 KB per frame vs ~250 KB
// for UXGA JPEG) so it's strictly cheaper on PSRAM.
bool camera_start_motion();

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

// Persist the current sensor settings to NVS (namespace "cam"). Call after
// a successful camera_apply_settings() in the HTTP handler.
void camera_save_settings();

// Load any saved settings from NVS and apply to the sensor. Call once at
// boot, after camera_start(). No-op when NVS is empty (sensor keeps the
// defaults set by camera_start).
void camera_load_and_apply_settings();

// Clear the "cam" NVS namespace. After this, the next boot loads no
// overrides and the sensor uses camera_start()'s defaults.
void camera_clear_saved_settings();

// Flash LED PWM (single GPIO, drives both LEDs in parallel). duty is 0..255.
void flash_led_init();
void flash_led_set(uint8_t duty);
uint8_t flash_led_get();

// Persist / load the current flash duty in NVS (namespace "flash").
void flash_led_save();
void flash_led_load();
