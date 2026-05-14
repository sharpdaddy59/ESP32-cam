// device_name.cpp — mDNS hostname management with NVS persistence.
// Ported from cores3-hydro with the MDNS_HOSTNAME base swapped to esp32s3-cam.

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <esp_mac.h>
#include <ctype.h>

#include "device_name.h"
#include "config.h"

static const char *NS_DEVICE = "device";
static const char *KEY_HOST  = "host";

static String s_default;
static String s_override;
static String s_active;
static String s_mac;

static void compute_default() {
  // Read straight from efuse — WiFi.macAddress() returns all-zero until
  // WiFi.mode() has been called, and we run before net_begin(). esp_read_mac
  // doesn't depend on the peripheral being initialized.
  uint8_t raw[6] = {0};
  esp_read_mac(raw, ESP_MAC_WIFI_STA);
  char compact_buf[13];
  snprintf(compact_buf, sizeof(compact_buf), "%02x%02x%02x%02x%02x%02x",
           raw[0], raw[1], raw[2], raw[3], raw[4], raw[5]);
  String compact = compact_buf;
  s_mac = compact;

  String suffix;
  if (compact.length() >= 4) {
    suffix = compact.substring(compact.length() - 4);
  } else {
    suffix = compact;
  }

  if (suffix.length() == 0 || suffix == "0000") {
    s_default = MDNS_HOSTNAME;
  } else {
    s_default = String(MDNS_HOSTNAME) + "-" + suffix;
  }
}

const char *device_hostname_validate(const char *name) {
  if (name == nullptr) return "name missing";
  size_t len = strlen(name);
  if (len == 0)  return "name is empty";
  if (len > 63)  return "name too long (max 63)";

  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    bool ok = (c >= 'a' && c <= 'z')
           || (c >= '0' && c <= '9')
           || (c == '-');
    if (!ok) return "only a-z, 0-9, and - allowed";
  }
  if (name[0] == '-')        return "must not start with -";
  if (name[len - 1] == '-')  return "must not end with -";
  return nullptr;
}

void device_name_init() {
  compute_default();

  Preferences prefs;
  String stored;
  if (prefs.begin(NS_DEVICE, /*readOnly=*/true)) {
    stored = prefs.getString(KEY_HOST, "");
    prefs.end();
  }

  if (stored.length() > 0) {
    const char *err = device_hostname_validate(stored.c_str());
    if (err) {
      Serial.printf("[device] stored hostname '%s' invalid (%s); ignoring\n",
                    stored.c_str(), err);
      stored = "";
    }
  }

  s_override = stored;
  s_active   = (s_override.length() > 0) ? s_override : s_default;

  Serial.printf("[device] hostname: %s (default %s, mac %s)\n",
                s_active.c_str(), s_default.c_str(), s_mac.c_str());
}

const char *device_hostname()         { return s_active.c_str(); }
const char *device_hostname_default() { return s_default.c_str(); }
const char *device_mac()              { return s_mac.c_str(); }

bool device_hostname_set(const char *name) {
  if (name == nullptr) return false;
  String candidate(name);
  candidate.trim();
  for (size_t i = 0; i < candidate.length(); i++) {
    candidate[i] = (char)tolower((unsigned char)candidate[i]);
  }

  if (candidate.length() == 0) {
    Preferences prefs;
    if (!prefs.begin(NS_DEVICE, /*readOnly=*/false)) {
      Serial.println("[device] NVS open failed (clear)");
      return false;
    }
    prefs.remove(KEY_HOST);
    prefs.end();
    s_override = "";
    s_active   = s_default;
    Serial.printf("[device] hostname cleared; using default %s\n", s_default.c_str());
    return true;
  }

  const char *err = device_hostname_validate(candidate.c_str());
  if (err) {
    Serial.printf("[device] reject hostname '%s': %s\n", candidate.c_str(), err);
    return false;
  }

  Preferences prefs;
  if (!prefs.begin(NS_DEVICE, /*readOnly=*/false)) {
    Serial.println("[device] NVS open failed (set)");
    return false;
  }
  prefs.putString(KEY_HOST, candidate);
  prefs.end();

  s_override = candidate;
  s_active   = s_override;
  Serial.printf("[device] hostname set to %s\n", s_active.c_str());
  return true;
}
