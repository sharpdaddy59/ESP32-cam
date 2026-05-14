// sd.h — SD card mount + snapshot writer.
//
// The SD card is entirely optional on V2 — every call here is a no-op
// when no card is present, returning false / 0 / "" / "[]" as appropriate.
// Callers should not treat absent-card as an error.
//
// 1-bit MMC mode. See config.h for the GPIO4 conflict that forces this.

#pragma once

#include <Arduino.h>

// Try to mount the SD card. Returns true on success, false if no card or
// any mount error. Idempotent. Logs once at INFO level; safe to ignore the
// return value if SD is optional for your flow.
bool sd_mount();

// Release the card. Idempotent.
void sd_unmount();

bool sd_mounted();

// Total / free space in bytes. Returns 0 if not mounted.
uint64_t sd_total_bytes();
uint64_t sd_free_bytes();

// Save a JPEG to /snapshots/YYYY-MM-DD/HHMMSS-<ms>.jpg. Returns the path
// on success, empty string on failure or when SD isn't mounted.
String sd_save_snapshot(const uint8_t *jpg, size_t len);

// JSON directory listing of `path`. Returns "[]" if SD isn't mounted or
// the path doesn't exist.
String sd_list_json(const char *path);
