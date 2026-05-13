// net.h — WiFi, mDNS, OTA plumbing for the ESP32-S3-CAM.
//
// Simpler than cores3-hydro's net.* because:
//   - no captive portal (creds come from SD via wifi_setup)
//   - no display
//   - NTP retained for snapshot timestamps
//
// Call net_begin() once during setup() AFTER device_name_init() and the
// initial sd_mount() / wifi_creds_load(). Call net_loop() from loop().

#pragma once

#include <Arduino.h>

// Bring up WiFi from the loaded creds. Non-blocking — if the connect fails
// or no creds are loaded, the reconnect machine retries in the background.
void net_begin();

// Call from loop(). Drives the reconnect machine, OTA polling, NTP sync.
void net_loop();

// True if WiFi is currently associated and has an IP.
bool net_is_connected();

int  net_rssi();

// Wipe /config/wifi.json on SD and reboot.
void net_reset_credentials();

// Re-announce mDNS and update OTA hostname after a runtime hostname change.
void net_apply_hostname_change();
