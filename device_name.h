// device_name.h — runtime mDNS hostname, with NVS-persisted user override.
// Ported from cores3-hydro.
//
// Default-with-MAC-suffix on first boot: "esp32s3-cam-a1b2" (last 4 hex
// chars of WiFi STA MAC). User can override via POST /hostname?name=<label>.
// The override is persisted in NVS.
//
// NVS namespace: "device". Key: "host".

#pragma once

void device_name_init();
const char *device_hostname();
const char *device_hostname_default();
const char *device_mac();
const char *device_hostname_validate(const char *name);
bool device_hostname_set(const char *name);
