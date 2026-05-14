// motion.cpp — mode state machine + motion-detection task.
//
// Design notes:
//
// 1. The motion task is spawned once at boot and runs forever. It checks
//    the current mode at the top of each iteration; if not MOTION, it
//    just yields. We don't tear down / spawn the task on mode changes —
//    keeps things simpler and the idle iteration is cheap.
//
// 2. Mode transitions that change camera config (in/out of MOTION) call
//    camera_stop() + camera_start_*() on the caller's task. Stream tasks
//    will see fb_get() return NULL during this window; that's OK — they
//    log and return. The 200 ms drain delay below is a courtesy so an
//    in-flight stream connection sees the mode change cleanly.
//
// 3. The reference frame for SAD comparison is "previous frame", not
//    "initial baseline". This catches movement without false-triggering
//    on slow lighting drift. Trade-off: a single still object placed in
//    the frame won't trigger on its own (only when it's moved). Adequate
//    for most "is anyone there" use cases.
//
// 4. The webhook POST is synchronous on the motion task — a slow webhook
//    will block detection for the duration of the HTTP call. Acceptable
//    because triggers are rare (cooldown enforces gaps) and a hanging
//    webhook just means a delayed next detection, not a crash.

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_camera.h>
#include <img_converters.h>

#include "motion.h"
#include "config.h"
#include "camera.h"
#include "sd.h"
#include "device_name.h"
#include "net.h"
#include "http_server.h"

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static const char *NS_MODE   = "mode";
static const char *NS_MOTION = "motion";

static const size_t MOTION_W = 320;
static const size_t MOTION_H = 240;
static const size_t MOTION_PIXELS = MOTION_W * MOTION_H;

static volatile Mode s_mode = MODE_STREAM;   // overwritten by NVS in motion_init

static SemaphoreHandle_t s_state_mutex = nullptr;
static TaskHandle_t      s_task        = nullptr;

// Config (cached from NVS; setters write through)
static uint8_t s_threshold  = 15;
static int     s_cooldown_s = 30;
static String  s_webhook;

// Working state
static uint8_t *s_reference          = nullptr;   // previous grayscale frame
static bool     s_reference_seeded   = false;
static uint8_t *s_last_jpg           = nullptr;   // most recent triggered JPEG
static size_t   s_last_jpg_len       = 0;
static uint32_t s_last_trigger_time  = 0;         // unix epoch
static uint32_t s_triggers_total     = 0;
static int      s_last_sad           = 0;         // average pixel diff (0..255)
static uint32_t s_cooldown_until_ms  = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void persist_mode(Mode m) {
  Preferences prefs;
  if (!prefs.begin(NS_MODE, /*readOnly=*/false)) return;
  prefs.putInt("m", (int)m);
  prefs.end();
}

static Mode load_mode() {
  Preferences prefs;
  if (!prefs.begin(NS_MODE, /*readOnly=*/true)) return MODE_STREAM;
  int v = prefs.getInt("m", (int)MODE_STREAM);
  prefs.end();
  if (v < MODE_IDLE || v > MODE_MOTION) v = MODE_STREAM;
  return (Mode)v;
}

static void load_config_from_nvs() {
  Preferences prefs;
  if (!prefs.begin(NS_MOTION, /*readOnly=*/true)) return;
  s_threshold  = prefs.getUChar("threshold",   15);
  s_cooldown_s = prefs.getInt("cooldown_s",    30);
  s_webhook    = prefs.getString("webhook",    "");
  prefs.end();
}

static void persist_config() {
  Preferences prefs;
  if (!prefs.begin(NS_MOTION, /*readOnly=*/false)) return;
  prefs.putUChar("threshold",   s_threshold);
  prefs.putInt("cooldown_s",    s_cooldown_s);
  prefs.putString("webhook",    s_webhook);
  prefs.end();
}

// Fire-and-mostly-forget webhook POST.
static void send_webhook(int sad) {
  if (s_webhook.length() == 0)  return;
  if (!net_is_connected())      return;

  HTTPClient http;
  if (!http.begin(s_webhook)) {
    Serial.println("[motion] webhook: begin failed");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(5000);

  JsonDocument doc;
  doc["event"]         = "motion";
  doc["timestamp"]     = (uint32_t)s_last_trigger_time;
  doc["uptime_s"]      = (uint32_t)(millis() / 1000);
  doc["hostname"]      = device_hostname();
  doc["sad"]           = sad;
  doc["threshold"]     = s_threshold;
  doc["image_url"]     = String("http://") + WiFi.localIP().toString() + "/motion/last.jpg";
  String body;
  serializeJson(doc, body);

  int code = http.POST(body);
  if (code > 0) Serial.printf("[motion] webhook → HTTP %d\n", code);
  else          Serial.printf("[motion] webhook failed: %s\n",
                              HTTPClient::errorToString(code).c_str());
  http.end();
}

// Slow-path trigger handler. Runs AFTER the camera fb has been returned to
// the driver pool, so we can do unbounded-duration work (SD writes, webhook
// POSTs that can take seconds) without holding a frame buffer and starving
// the camera DMA — which is what produced the cam_hal FB-OVF storm in the
// previous version.
//
// `jpg` is malloc'd (frame2jpg's allocator). We take ownership: copy into
// PSRAM for the long-lived s_last_jpg, then free the temporary.
static void handle_trigger(uint8_t *jpg, size_t jpg_len, int sad) {
  Serial.printf("[motion] trigger! avg-diff=%d threshold=%d\n", sad, s_threshold);

  time_t now;
  time(&now);

  // Stash in PSRAM under the mutex so /motion/last.jpg can read it safely.
  xSemaphoreTake(s_state_mutex, portMAX_DELAY);
  s_last_trigger_time = (uint32_t)now;
  s_triggers_total++;
  uint8_t *ps = (uint8_t *)ps_malloc(jpg_len);
  if (ps) {
    memcpy(ps, jpg, jpg_len);
    if (s_last_jpg) free(s_last_jpg);
    s_last_jpg     = ps;
    s_last_jpg_len = jpg_len;
  } else {
    Serial.println("[motion] PSRAM alloc for trigger frame failed");
  }
  xSemaphoreGive(s_state_mutex);

  // Slow work outside the mutex.
  if (sd_mounted()) {
    sd_save_snapshot(jpg, jpg_len);
  }
  send_webhook(sad);

  free(jpg);

  s_cooldown_until_ms = millis() + (uint32_t)s_cooldown_s * 1000UL;
}

// ---------------------------------------------------------------------------
// Motion task — always running, gated by mode.
// ---------------------------------------------------------------------------
static void motion_task(void *) {
  for (;;) {
    if (s_mode != MODE_MOTION) {
      s_reference_seeded = false;   // re-seed when re-entering motion mode
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    // Sanity: in motion mode the format should be GRAYSCALE and len == 76800.
    // If a mode-change race has us looking at a JPEG buffer, skip.
    if (fb->format != PIXFORMAT_GRAYSCALE || fb->len < MOTION_PIXELS) {
      esp_camera_fb_return(fb);
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    // Critical: do ONLY fast work while the camera fb is held. Everything
    // slow (SD writes, webhook HTTP POST) happens after the fb is back in
    // the camera pool. Holding fb during slow work was the cause of the
    // cam_hal FB-OVF storm — DMA had no buffer to drain into.
    bool will_trigger = false;
    int  sad_value    = 0;
    uint8_t *trigger_jpg = nullptr;
    size_t   trigger_jpg_len = 0;

    if (!s_reference_seeded) {
      memcpy(s_reference, fb->buf, MOTION_PIXELS);
      s_reference_seeded = true;
    } else {
      // Average per-pixel absolute difference. uint32_t SAD fits because
      // worst case is 320*240*255 = 19.6M, well below 2^32.
      uint32_t sad = 0;
      const uint8_t *cur = fb->buf;
      const uint8_t *ref = s_reference;
      for (size_t i = 0; i < MOTION_PIXELS; i++) {
        int d = (int)cur[i] - (int)ref[i];
        sad += (d < 0) ? -d : d;
      }
      uint32_t avg = sad / MOTION_PIXELS;
      s_last_sad = (int)avg;
      sad_value  = (int)avg;

      bool in_cooldown = (int32_t)(millis() - s_cooldown_until_ms) < 0;
      if (avg > (uint32_t)s_threshold && !in_cooldown) {
        // Encode the frame to a temporary JPEG while we still hold fb,
        // then release the fb before doing anything slow.
        if (frame2jpg(fb, 80, &trigger_jpg, &trigger_jpg_len)
            && trigger_jpg && trigger_jpg_len > 0) {
          will_trigger = true;
        } else {
          Serial.println("[motion] frame2jpg failed");
          if (trigger_jpg) { free(trigger_jpg); trigger_jpg = nullptr; }
        }
      }

      // Reference always advances — "diff from previous frame" semantics.
      memcpy(s_reference, fb->buf, MOTION_PIXELS);
    }

    // Return the frame buffer NOW — slow trigger work happens below with
    // no fb held, so the DMA always has buffers to rotate through.
    esp_camera_fb_return(fb);

    if (will_trigger) {
      handle_trigger(trigger_jpg, trigger_jpg_len, sad_value);
      // handle_trigger took ownership of trigger_jpg and freed it.
    }

    vTaskDelay(pdMS_TO_TICKS(500));   // ~2 fps
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void motion_init() {
  s_state_mutex = xSemaphoreCreateMutex();
  if (!s_state_mutex) {
    Serial.println("[motion] FATAL: mutex create failed");
    return;
  }

  s_reference = (uint8_t *)ps_malloc(MOTION_PIXELS);
  if (!s_reference) {
    Serial.println("[motion] FATAL: reference-frame PSRAM alloc failed");
    return;
  }

  load_config_from_nvs();
  s_mode = load_mode();
  Serial.printf("[motion] mode=%d threshold=%u cooldown=%ds webhook=%s\n",
                (int)s_mode, s_threshold, s_cooldown_s,
                s_webhook.length() ? s_webhook.c_str() : "(none)");

  // Pinned to core 1 so we don't compete with the Wi-Fi/lwIP task on core 0.
  // 6 KB stack is comfortable; HTTPClient + ArduinoJson together fit.
  xTaskCreatePinnedToCore(motion_task, "motion", 6144, nullptr, 2,
                          &s_task, 1);
}

Mode motion_get_mode() { return s_mode; }

bool motion_set_mode(Mode new_mode) {
  if (new_mode < MODE_IDLE || new_mode > MODE_MOTION) return false;
  if (new_mode == s_mode) return true;

  Mode old_mode = s_mode;
  Serial.printf("[motion] mode change %d -> %d\n", (int)old_mode, (int)new_mode);

  // Camera reconfig is only needed when entering or leaving MOTION mode
  // (STREAM and IDLE share the same JPEG-mode camera config).
  bool need_reconfig = (old_mode == MODE_MOTION) != (new_mode == MODE_MOTION);

  if (need_reconfig) {
    // Park in IDLE for the duration of the reconfig. Two reasons:
    //   1. The motion task's loop body gates on (s_mode == MODE_MOTION) and
    //      calls esp_camera_fb_get() inside it. Setting s_mode = IDLE here
    //      makes the next loop iteration skip the fb_get entirely.
    //   2. The stream filler's mode check at top exits when s_mode != STREAM,
    //      stopping fresh fb_get calls from /stream.
    // We then sleep 700 ms to give either task a chance to complete any
    // in-flight fb_get (~one camera frame interval + one task tick) before
    // the camera driver is torn down. Without this, deinit can free the
    // frame queue while another task is blocked in xQueueReceive on it,
    // which asserts.
    s_mode = MODE_IDLE;
    vTaskDelay(pdMS_TO_TICKS(700));

    // Any held s_stream_fb pointer in http_server is about to become a
    // dangling reference once camera_deinit() runs. Null it out without
    // calling fb_return — the underlying buffer is about to be freed by
    // the camera driver anyway.
    http_server_invalidate_stream_state();

    camera_stop();
    bool ok = (new_mode == MODE_MOTION) ? camera_start_motion()
                                        : camera_start();
    if (!ok) {
      Serial.println("[motion] camera reconfig failed; trying STREAM fallback");
      camera_stop();
      if (!camera_start()) {
        // Both new-mode init AND the fallback failed. This almost always
        // means the DRAM heap is too fragmented to allocate a fresh 32 KB
        // DMA buffer — typically a consequence of a prior FB-OVF storm
        // leaving the camera driver's allocator in a bad state. The only
        // reliable recovery is a reboot; the camera initializes cleanly
        // from a fresh heap.
        Serial.println("[motion] FALLBACK also failed — rebooting to recover");
        delay(500);
        ESP.restart();
      }
      s_mode = MODE_STREAM;
      persist_mode(MODE_STREAM);
      return false;
    }
    // Reapply user-tuned camera settings (brightness etc.) — they're
    // pixel-format-agnostic and survive a deinit/init cycle.
    camera_load_and_apply_settings();
  }

  // Commit the new mode now that any camera reconfig is complete. The
  // motion task's next iteration will see MOTION and start grabbing on
  // the freshly-initialized grayscale camera (or stop grabbing for IDLE
  // or STREAM).
  s_mode = new_mode;
  persist_mode(new_mode);
  return true;
}

uint8_t motion_get_threshold()    { return s_threshold; }
int     motion_get_cooldown_s()   { return s_cooldown_s; }
String  motion_get_webhook()      { return s_webhook; }

void motion_set_threshold(uint8_t v) {
  s_threshold = v;
  persist_config();
}
void motion_set_cooldown_s(int s) {
  if (s < 0) s = 0;
  if (s > 3600) s = 3600;
  s_cooldown_s = s;
  persist_config();
}
void motion_set_webhook(const String &url) {
  s_webhook = url;
  persist_config();
}

int      motion_last_sad()           { return s_last_sad; }
uint32_t motion_last_trigger_time()  { return s_last_trigger_time; }
uint32_t motion_trigger_count()      { return s_triggers_total; }

bool motion_copy_last_jpeg(uint8_t **out_buf, size_t *out_len) {
  xSemaphoreTake(s_state_mutex, portMAX_DELAY);
  if (!s_last_jpg || s_last_jpg_len == 0) {
    xSemaphoreGive(s_state_mutex);
    return false;
  }
  uint8_t *copy = (uint8_t *)ps_malloc(s_last_jpg_len);
  if (!copy) {
    xSemaphoreGive(s_state_mutex);
    return false;
  }
  memcpy(copy, s_last_jpg, s_last_jpg_len);
  *out_buf = copy;
  *out_len = s_last_jpg_len;
  xSemaphoreGive(s_state_mutex);
  return true;
}
