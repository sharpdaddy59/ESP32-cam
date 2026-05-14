// net.h — WiFi (via WiFiManager captive portal), mDNS, OTA, NTP.
//
// Provisioning flow:
//   - Boot calls net_begin() which calls WiFiManager.autoConnect().
//   - If creds are stored in NVS: connect normally.
//   - If creds are missing or stale: open an open AP "esp32cam-setup-XXXX",
//     present a captive portal at 192.168.4.1, accept SSID/password,
//     persist them in NVS, reboot to use them.
//   - If the portal times out with no client: reboot to retry.
//
// net_begin() BLOCKS until WiFi is connected or the portal times out.
// http_server_begin() MUST be called after net_begin() — both want port 80.

#pragma once

#include <Arduino.h>

void net_begin();
void net_loop();

bool net_is_connected();
int  net_rssi();

// Wipe NVS-stored creds (WiFiManager + our own preferences) and reboot.
void net_reset_credentials();

// Re-announce mDNS and OTA after a hostname change.
void net_apply_hostname_change();
