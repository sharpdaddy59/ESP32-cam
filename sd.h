// sd.h — SD card mount, wifi.json reader, snapshot writer.
//
// The SD card is shared between the firmware and USB MSC. usb_msc.cpp owns
// the handoff: when a USB host enumerates, the firmware unmounts via
// sd_unmount(); when it disconnects, sd_mount() is called again.
// Call sites that touch SD must check sd_mounted() first.

#pragma once

#include <Arduino.h>

struct WifiCreds {
  String ssid;
  String password;
  String hostname;  // optional — empty means use default per-MAC name
};

// Mount the SD card via SD_MMC in 1-bit mode. Returns true on success.
// Idempotent — calling when already mounted is a no-op.
bool sd_mount();

// Release the card so USB MSC can claim it. Idempotent.
void sd_unmount();

bool sd_mounted();

// Total / free space in bytes. Returns 0 if not mounted.
uint64_t sd_total_bytes();
uint64_t sd_free_bytes();

// Read /config/wifi.json into the given creds struct. Returns true on
// success. Returns false (and logs) if the file is missing, unreadable, or
// not valid JSON. The hostname field is optional.
bool sd_load_wifi_creds(WifiCreds &out);

// Delete /config/wifi.json (used by /wifi/reset). Returns true if removed
// or already absent.
bool sd_clear_wifi_creds();

// Save a JPEG to /snapshots/YYYY-MM-DD/HHMMSS.jpg (or a synthetic name if
// time isn't yet synced). Returns the path written on success, empty on
// failure. The directory is created if missing.
String sd_save_snapshot(const uint8_t *jpg, size_t len);

// JSON directory listing of `path` — returns a JSON array string like:
//   [{"name":"foo.jpg","size":12345,"dir":false}, ...]
// Returns "[]" on failure. Path defaults to "/".
String sd_list_json(const char *path);
