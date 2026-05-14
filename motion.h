// motion.h — mode state machine + motion-detection task.
//
// Three top-level modes, persisted in NVS (namespace "mode"):
//   IDLE    — camera initialized, no stream, /snapshot works on demand.
//             Lowest power / heat baseline.
//   STREAM  — current default: MJPEG /stream + /snapshot, JPEG framesize.
//   MOTION  — camera reconfigured to GRAYSCALE @ QVGA. A FreeRTOS task
//             runs at 2 fps, compares the current frame to the previous
//             via average per-pixel absolute difference (SAD/pixels),
//             triggers when above threshold. Trigger artifacts: cached
//             JPEG at /motion/last.jpg, optional SD save, optional
//             webhook POST.
//
// Mode transitions:
//   IDLE↔STREAM — instant (same camera config).
//   *↔MOTION    — slow (~1 s) because the camera needs deinit + re-init
//                 to switch pixel format. User-driven, rare.
//
// Tunables (NVS namespace "motion"):
//   threshold (uint8) — average per-pixel diff (0..255) that counts as
//                       motion. Smaller = more sensitive. Default 15.
//   cooldown_s        — minimum seconds between consecutive triggers.
//   webhook           — POST URL fired on trigger (empty = disabled).

#pragma once

#include <Arduino.h>

enum Mode {
  MODE_IDLE   = 0,
  MODE_STREAM = 1,
  MODE_MOTION = 2,
};

// Initialize NVS-backed state, spawn the motion FreeRTOS task. Call from
// setup() AFTER camera_start() and after WiFi has come up enough for the
// webhook to work (only the webhook needs WiFi; everything else runs
// regardless).
void motion_init();

// Current mode.
Mode motion_get_mode();

// Set mode and persist. Returns true on success. May reconfigure the
// camera (slow if mode transitions in/out of MOTION). Safe to call from
// any task — the heavy work happens on the calling task's stack, not the
// motion task.
bool motion_set_mode(Mode new_mode);

// Config getters/setters. Setters persist to NVS.
uint8_t motion_get_threshold();
int     motion_get_cooldown_s();
String  motion_get_webhook();
void    motion_set_threshold(uint8_t v);
void    motion_set_cooldown_s(int s);
void    motion_set_webhook(const String &url);

// Most recent SAD value (smoothed). For status/diagnostics.
int      motion_last_sad();
// Unix epoch of the last trigger; 0 if never.
uint32_t motion_last_trigger_time();
// Total triggers since boot.
uint32_t motion_trigger_count();

// Copy the most-recently-triggered JPEG into a caller-provided buffer-pointer
// outputs. The caller must free *out_buf with free() when done.
// Returns true if a triggered frame exists; false if nothing has triggered
// yet (or PSRAM alloc failed).
bool motion_copy_last_jpeg(uint8_t **out_buf, size_t *out_len);
