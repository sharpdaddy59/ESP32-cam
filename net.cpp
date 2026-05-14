// net.cpp — WiFi, mDNS, OTA, NTP for esp32-cam-v2.
//
// Wi-Fi provisioning is handed off to WiFiManager. It owns the captive
// portal and its own NVS namespace for credentials, so we don't read or
// write SSID/password ourselves. Our reconnect machine still runs once the
// connection is up — WiFiManager doesn't monitor disconnects.

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <time.h>

#include "net.h"
#include "config.h"
#include "device_name.h"

enum class WifiPhase {
  Boot,
  Connected,
  Disconnected,
};

static WifiPhase s_phase = WifiPhase::Boot;
static int       s_attempt = 0;
static uint32_t  s_next_action_ms = 0;

static volatile bool s_services_pending = false;
static bool s_ota_started = false;
static bool s_ntp_synced  = false;
static uint32_t s_last_ntp_attempt_ms = 0;

static volatile bool s_connected = false;
static volatile int  s_rssi      = 0;

static const uint32_t NTP_RESYNC_INTERVAL_MS = 24UL * 60UL * 60UL * 1000UL;
static const uint32_t NTP_RETRY_INTERVAL_MS  =  5UL * 60UL * 1000UL;

static uint32_t backoff_for_attempt(int n) {
  switch (n) {
    case 1:  return WIFI_BACKOFF_1_MS;
    case 2:  return WIFI_BACKOFF_2_MS;
    case 3:  return WIFI_BACKOFF_3_MS;
    case 4:  return WIFI_BACKOFF_4_MS;
    case 5:  return WIFI_BACKOFF_5_MS;
    default: return WIFI_BACKOFF_CAP_MS;
  }
}

static void try_ntp_sync() {
  s_last_ntp_attempt_ms = millis();
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  struct tm tm_check;
  if (!getLocalTime(&tm_check, 5000)) {
    Serial.println("[net] NTP sync timed out");
    return;
  }
  s_ntp_synced = true;
  time_t now;
  time(&now);
  Serial.printf("[net] NTP synced; epoch=%lu\n", (unsigned long)now);
}

static void start_services() {
  const char *host = device_hostname();
  MDNS.end();
  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.printf("[net] mDNS up: http://%s.local\n", host);
  } else {
    Serial.println("[net] mDNS begin FAILED");
  }
  ArduinoOTA.setHostname(host);
  ArduinoOTA.begin();
  s_ota_started = true;
  Serial.println("[net] OTA up");
  if (!s_ntp_synced) try_ntp_sync();
}

static void on_wifi_event(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      s_connected = true;
      s_rssi      = WiFi.RSSI();
      s_phase     = WifiPhase::Connected;
      s_attempt   = 0;
      s_services_pending = true;
      Serial.printf("[net] connected: %s  (rssi %d)\n",
                    WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      s_connected = false;
      if (s_phase == WifiPhase::Connected) {
        s_phase   = WifiPhase::Disconnected;
        s_attempt = 1;
        s_next_action_ms = millis() + backoff_for_attempt(1);
        Serial.printf("[net] disconnected; retry in %lu s\n",
                      (unsigned long)(backoff_for_attempt(1) / 1000));
      }
      break;

    default:
      break;
  }
}

// Construct AP name with the same MAC suffix the runtime hostname uses, so
// you can tell two boards apart in the WiFi picker.
static String build_ap_name() {
  String mac = device_mac();   // 12-hex, lowercase
  String suffix = mac.length() >= 4 ? mac.substring(mac.length() - 4) : mac;
  return String(WM_AP_PREFIX) + "-" + suffix;
}

void net_begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.persistent(true);   // WiFiManager wants creds persisted to NVS
  WiFi.onEvent(on_wifi_event);
  WiFi.setHostname(device_hostname());

  WiFiManager wm;
  wm.setConfigPortalTimeout(WM_PORTAL_TIMEOUT_S);
  // Don't auto-close the portal on first failed connect attempt — give the
  // user a chance to fix a typo without rebooting.
  wm.setBreakAfterConfig(true);

  String ap = build_ap_name();
  Serial.printf("[net] WiFiManager: trying saved creds, AP fallback '%s'\n", ap.c_str());

  if (!wm.autoConnect(ap.c_str())) {
    Serial.println("[net] WiFiManager: portal timed out / connect failed; rebooting to retry");
    delay(500);
    ESP.restart();
  }
  // Connected. GOT_IP event will have fired and set s_phase = Connected.
}

void net_loop() {
  uint32_t now = millis();

  if (s_services_pending) {
    s_services_pending = false;
    start_services();
  }

  switch (s_phase) {
    case WifiPhase::Disconnected:
      if ((int32_t)(now - s_next_action_ms) >= 0) {
        Serial.printf("[net] reconnect attempt %d\n", s_attempt);
        WiFi.reconnect();
        s_attempt++;
        s_next_action_ms = now + backoff_for_attempt(s_attempt);
      }
      break;

    case WifiPhase::Connected:
      s_rssi = WiFi.RSSI();
      {
        uint32_t interval = s_ntp_synced ? NTP_RESYNC_INTERVAL_MS : NTP_RETRY_INTERVAL_MS;
        if ((now - s_last_ntp_attempt_ms) > interval) try_ntp_sync();
      }
      break;

    default:
      break;
  }

  if (s_ota_started) ArduinoOTA.handle();
}

bool net_is_connected() { return s_connected; }
int  net_rssi()         { return s_rssi; }

void net_reset_credentials() {
  Serial.println("[net] wiping creds and rebooting");
  WiFiManager wm;
  wm.resetSettings();
  delay(200);
  ESP.restart();
}

void net_apply_hostname_change() {
  if (!s_connected) {
    Serial.println("[net] hostname change deferred — WiFi not connected");
    return;
  }
  const char *host = device_hostname();
  WiFi.setHostname(host);
  MDNS.end();
  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", HTTP_PORT);
    Serial.printf("[net] mDNS re-announced: http://%s.local\n", host);
  }
  ArduinoOTA.setHostname(host);
}
