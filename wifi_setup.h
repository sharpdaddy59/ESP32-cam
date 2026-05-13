// wifi_setup.h — read Wi-Fi credentials from /config/wifi.json on SD.
//
// Unlike cores3-hydro, there is no captive portal. The user puts a small
// JSON file on the SD card (via USB MSC or by removing the card), and the
// firmware reads it at boot.
//
// JSON shape:
//   { "ssid": "...", "password": "...", "hostname": "optional-name" }

#pragma once

#include "sd.h"

// Read creds from SD into `out`. Returns true if a valid file was found.
// On failure, the caller should leave WiFi un-started and rely on USB MSC
// to let the user place the file.
bool wifi_creds_load(WifiCreds &out);

// Delete /config/wifi.json. Returns true on success or if already absent.
bool wifi_creds_clear();
