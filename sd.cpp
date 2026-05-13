// sd.cpp — SD card mount, wifi.json reader, snapshot writer.

#include <Arduino.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include <time.h>

#include "sd.h"
#include "config.h"

static bool s_mounted = false;

bool sd_mount() {
  if (s_mounted) return true;

  // Configure 1-bit mode pins. The S3 lets us pick — defaults to GPIO 14/15/2
  // which conflict with the camera. setPins() must be called before begin().
  if (!SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0)) {
    Serial.println("[sd] setPins failed");
    return false;
  }

  // 1-bit mode, mount as "/sdcard" (default), format-on-fail disabled.
  if (!SD_MMC.begin("/sdcard", /*mode1bit=*/true)) {
    Serial.println("[sd] mount failed");
    return false;
  }

  uint8_t type = SD_MMC.cardType();
  if (type == CARD_NONE) {
    Serial.println("[sd] no card detected");
    SD_MMC.end();
    return false;
  }

  s_mounted = true;
  Serial.printf("[sd] mounted (%lluMB total / %lluMB free)\n",
                SD_MMC.totalBytes() / (1024ULL * 1024ULL),
                (SD_MMC.totalBytes() - SD_MMC.usedBytes()) / (1024ULL * 1024ULL));
  return true;
}

void sd_unmount() {
  if (!s_mounted) return;
  SD_MMC.end();
  s_mounted = false;
  Serial.println("[sd] unmounted");
}

bool sd_mounted() { return s_mounted; }

uint64_t sd_total_bytes() {
  if (!s_mounted) return 0;
  return SD_MMC.totalBytes();
}

uint64_t sd_free_bytes() {
  if (!s_mounted) return 0;
  return SD_MMC.totalBytes() - SD_MMC.usedBytes();
}

bool sd_load_wifi_creds(WifiCreds &out) {
  if (!s_mounted) {
    Serial.println("[sd] load_wifi_creds: not mounted");
    return false;
  }

  File f = SD_MMC.open(SD_PATH_WIFI_JSON, FILE_READ);
  if (!f) {
    Serial.printf("[sd] %s not found\n", SD_PATH_WIFI_JSON);
    return false;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("[sd] wifi.json parse error: %s\n", err.c_str());
    return false;
  }

  const char *ssid = doc["ssid"] | "";
  const char *pwd  = doc["password"] | "";
  const char *host = doc["hostname"] | "";
  if (!ssid || !*ssid) {
    Serial.println("[sd] wifi.json missing 'ssid'");
    return false;
  }

  out.ssid     = ssid;
  out.password = pwd;
  out.hostname = host;
  Serial.printf("[sd] loaded wifi creds for SSID '%s'\n", ssid);
  return true;
}

bool sd_clear_wifi_creds() {
  if (!s_mounted) return false;
  if (!SD_MMC.exists(SD_PATH_WIFI_JSON)) return true;
  return SD_MMC.remove(SD_PATH_WIFI_JSON);
}

// Ensure /snapshots/YYYY-MM-DD/ exists, return that path. If time isn't
// synced yet, use /snapshots/unsynced/.
static String snapshot_dir_for_now() {
  time_t now;
  time(&now);
  String base = SD_PATH_SNAPSHOTS;

  if (!SD_MMC.exists(base)) SD_MMC.mkdir(base);

  if (now < 1700000000) {
    String d = base + "/unsynced";
    if (!SD_MMC.exists(d)) SD_MMC.mkdir(d);
    return d;
  }

  struct tm tm_local;
  localtime_r(&now, &tm_local);
  char dbuf[16];
  snprintf(dbuf, sizeof(dbuf), "%04d-%02d-%02d",
           tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday);
  String d = base + "/" + dbuf;
  if (!SD_MMC.exists(d)) SD_MMC.mkdir(d);
  return d;
}

String sd_save_snapshot(const uint8_t *jpg, size_t len) {
  if (!s_mounted) return "";
  if (!jpg || len == 0) return "";

  String dir = snapshot_dir_for_now();

  time_t now;
  time(&now);
  struct tm tm_local;
  localtime_r(&now, &tm_local);
  char fname[32];
  if (now >= 1700000000) {
    snprintf(fname, sizeof(fname), "%02d%02d%02d-%lu.jpg",
             tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec,
             (unsigned long)(millis() % 1000));
  } else {
    // No time — use millis() so files are at least unique within a session.
    snprintf(fname, sizeof(fname), "snap-%lu.jpg", (unsigned long)millis());
  }

  String path = dir + "/" + fname;
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    Serial.printf("[sd] open %s for write FAILED\n", path.c_str());
    return "";
  }
  size_t written = f.write(jpg, len);
  f.close();
  if (written != len) {
    Serial.printf("[sd] short write: %u/%u bytes\n",
                  (unsigned)written, (unsigned)len);
    return "";
  }
  Serial.printf("[sd] saved %s (%u bytes)\n", path.c_str(), (unsigned)len);
  return path;
}

String sd_list_json(const char *path) {
  if (!s_mounted) return "[]";
  if (!path || !*path) path = "/";

  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return "[]";
  }

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  File entry = dir.openNextFile();
  while (entry) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = String(entry.name());
    o["size"] = (uint32_t)entry.size();
    o["dir"]  = entry.isDirectory();
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();

  String out;
  serializeJson(arr, out);
  return out;
}
