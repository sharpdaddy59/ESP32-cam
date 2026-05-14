// camera.cpp — esp_camera wrapper + flash-LED PWM for the V2 (classic ESP32) board.
//
// Pin map is the AI-Thinker ESP32-CAM standard (matched 1:1 by the nulllab
// V2 PCB). The esp_camera library auto-detects the sensor at init time — the
// same code path serves OV2640 and OV3660 boards transparently. Don't rely
// on which one your unit shipped with.

#include <Arduino.h>
#include <Preferences.h>
#include <esp_camera.h>

#include "camera.h"
#include "config.h"

static const char *NS_CAM   = "cam";
static const char *NS_FLASH = "flash";

// AI-Thinker pin constants — duplicated locally so we don't depend on the
// esp32-camera examples folder being on the include path.
#define CAM_PIN_PWDN     32
#define CAM_PIN_RESET    -1
#define CAM_PIN_XCLK      0
#define CAM_PIN_SIOD     26
#define CAM_PIN_SIOC     27
#define CAM_PIN_D7       35
#define CAM_PIN_D6       34
#define CAM_PIN_D5       39
#define CAM_PIN_D4       36
#define CAM_PIN_D3       21
#define CAM_PIN_D2       19
#define CAM_PIN_D1       18
#define CAM_PIN_D0        5
#define CAM_PIN_VSYNC    25
#define CAM_PIN_HREF     23
#define CAM_PIN_PCLK     22

static bool s_running = false;

bool camera_start() {
  if (s_running) return true;

  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;   // XCLK PWM — kept clear of FLASH_LED_CHANNEL
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = CAM_PIN_D0;
  cfg.pin_d1       = CAM_PIN_D1;
  cfg.pin_d2       = CAM_PIN_D2;
  cfg.pin_d3       = CAM_PIN_D3;
  cfg.pin_d4       = CAM_PIN_D4;
  cfg.pin_d5       = CAM_PIN_D5;
  cfg.pin_d6       = CAM_PIN_D6;
  cfg.pin_d7       = CAM_PIN_D7;
  cfg.pin_xclk     = CAM_PIN_XCLK;
  cfg.pin_pclk     = CAM_PIN_PCLK;
  cfg.pin_vsync    = CAM_PIN_VSYNC;
  cfg.pin_href     = CAM_PIN_HREF;
  cfg.pin_sccb_sda = CAM_PIN_SIOD;
  cfg.pin_sccb_scl = CAM_PIN_SIOC;
  cfg.pin_pwdn     = CAM_PIN_PWDN;
  cfg.pin_reset    = CAM_PIN_RESET;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size   = FRAMESIZE_SVGA;
  cfg.jpeg_quality = 12;
  cfg.fb_count     = 1;
  cfg.fb_location  = CAMERA_FB_IN_DRAM;
  cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

  if (psramFound()) {
    // V2 has 2 MB quad PSRAM. fb_count=3 gives the stream and a concurrent
    // snapshot enough buffers that the camera driver always has at least
    // one to fill. With 2 buffers the snapshot path could hold one while
    // the stream held the other, starving the camera and stalling both.
    // 3 x ~250KB worst-case UXGA JPEG ≈ 750KB, well within 2MB.
    cfg.frame_size   = FRAMESIZE_UXGA;
    cfg.jpeg_quality = 10;
    cfg.fb_count     = 3;
    cfg.fb_location  = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode    = CAMERA_GRAB_LATEST;
  } else {
    Serial.println("[cam] WARN: PSRAM not found; staying at SVGA, single-buffer");
  }

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("[cam] init failed: 0x%x\n", err);
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    Serial.printf("[cam] sensor PID=0x%02x (OV2640=0x26, OV3660=0x36)\n",
                  s->id.PID);
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_special_effect(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_wb_mode(s, 0);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 1);
    s->set_ae_level(s, 0);
    s->set_aec_value(s, 300);
    s->set_gain_ctrl(s, 1);
    s->set_agc_gain(s, 0);
    s->set_gainceiling(s, (gainceiling_t)0);
    s->set_bpc(s, 0);
    s->set_wpc(s, 1);
    s->set_raw_gma(s, 1);
    s->set_lenc(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
    s->set_dcw(s, 1);
    s->set_colorbar(s, 0);

    // OV3660 quirks: starting brightness is a bit dim, vflip needed for the
    // typical orientation when the lens is folded over the module.
    if (s->id.PID == 0x36 /*OV3660_PID*/) {
      s->set_brightness(s, 1);
      s->set_saturation(s, -2);
    }
  }

  s_running = true;
  Serial.println("[cam] init OK");
  return true;
}

bool camera_start_motion() {
  if (s_running) return true;

  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = CAM_PIN_D0;
  cfg.pin_d1       = CAM_PIN_D1;
  cfg.pin_d2       = CAM_PIN_D2;
  cfg.pin_d3       = CAM_PIN_D3;
  cfg.pin_d4       = CAM_PIN_D4;
  cfg.pin_d5       = CAM_PIN_D5;
  cfg.pin_d6       = CAM_PIN_D6;
  cfg.pin_d7       = CAM_PIN_D7;
  cfg.pin_xclk     = CAM_PIN_XCLK;
  cfg.pin_pclk     = CAM_PIN_PCLK;
  cfg.pin_vsync    = CAM_PIN_VSYNC;
  cfg.pin_href     = CAM_PIN_HREF;
  cfg.pin_sccb_sda = CAM_PIN_SIOD;
  cfg.pin_sccb_scl = CAM_PIN_SIOC;
  cfg.pin_pwdn     = CAM_PIN_PWDN;
  cfg.pin_reset    = CAM_PIN_RESET;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_GRAYSCALE;
  cfg.frame_size   = FRAMESIZE_QVGA;     // 320x240 = 76,800 bytes
  // fb_count=3: with 2 buffers, holding one for the SAD work + brief
  // JPEG encode meant the DMA had only one buffer to rotate, and any
  // hiccup in our consumer cadence produced FB-OVF storms. 3 buffers
  // give DMA at least 2 to rotate even while we hold one. PSRAM cost:
  // ~76 KB extra (3 × QVGA grayscale). Trivial in 2 MB.
  cfg.fb_count     = 3;
  cfg.fb_location  = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  cfg.grab_mode    = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("[cam] motion-mode init failed: 0x%x\n", err);
    return false;
  }
  s_running = true;
  Serial.println("[cam] init OK (motion mode: GRAYSCALE @ QVGA)");
  return true;
}

void camera_stop() {
  if (!s_running) return;
  esp_camera_deinit();
  s_running = false;
  Serial.println("[cam] stopped");
}

bool camera_running() { return s_running; }

camera_fb_t *camera_grab() {
  if (!s_running) return nullptr;
  return esp_camera_fb_get();
}

bool camera_apply_settings(const CameraSettings &cs) {
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return false;

  if (cs.framesize  >= 0) s->set_framesize(s, (framesize_t)cs.framesize);
  if (cs.quality    >= 0) s->set_quality(s, cs.quality);
  if (cs.brightness >= -2) s->set_brightness(s, cs.brightness);
  if (cs.contrast   >= -2) s->set_contrast(s, cs.contrast);
  if (cs.saturation >= -2) s->set_saturation(s, cs.saturation);
  if (cs.hmirror    >= 0) s->set_hmirror(s, cs.hmirror);
  if (cs.vflip      >= 0) s->set_vflip(s, cs.vflip);
  if (cs.wb_mode    >= 0) s->set_wb_mode(s, cs.wb_mode);
  if (cs.ae_level   >= -2) s->set_ae_level(s, cs.ae_level);
  if (cs.agc_gain   >= 0) s->set_agc_gain(s, cs.agc_gain);
  return true;
}

CameraSettings camera_get_settings() {
  CameraSettings cs;
  sensor_t *s = esp_camera_sensor_get();
  if (!s) return cs;
  cs.framesize  = s->status.framesize;
  cs.quality    = s->status.quality;
  cs.brightness = s->status.brightness;
  cs.contrast   = s->status.contrast;
  cs.saturation = s->status.saturation;
  cs.hmirror    = s->status.hmirror;
  cs.vflip      = s->status.vflip;
  cs.wb_mode    = s->status.wb_mode;
  cs.ae_level   = s->status.ae_level;
  cs.agc_gain   = s->status.agc_gain;
  return cs;
}

// -- Flash LED -------------------------------------------------------------
static uint8_t s_flash_duty = 0;

void flash_led_init() {
  // esp32 core 3.x: ledcAttachChannel binds pin → channel + config in one call,
  // and ledcWrite() now takes the PIN, not the channel. We still pin the
  // channel explicitly (rather than using the auto-allocating ledcAttach())
  // so we don't race the camera's XCLK on channel 0.
  ledcAttachChannel(FLASH_LED_PIN, FLASH_LED_FREQ, FLASH_LED_RES_BITS, FLASH_LED_CHANNEL);
  ledcWrite(FLASH_LED_PIN, 0);
  s_flash_duty = 0;
}

void flash_led_set(uint8_t duty) {
  s_flash_duty = duty;
  ledcWrite(FLASH_LED_PIN, duty);
}

uint8_t flash_led_get() { return s_flash_duty; }

// ---------------------------------------------------------------------------
// NVS persistence
//
// CameraSettings uses -1 (and -100 for the centred -2..+2 sliders) as "leave
// unchanged" sentinels in camera_apply_settings. We use the same sentinels
// as the Preferences default values, so a missing NVS key means "don't
// touch that field" and the sensor keeps the boot defaults set in
// camera_start().
// ---------------------------------------------------------------------------

void camera_save_settings() {
  CameraSettings cs = camera_get_settings();
  Preferences prefs;
  if (!prefs.begin(NS_CAM, /*readOnly=*/false)) {
    Serial.println("[cam] NVS open failed (save)");
    return;
  }
  prefs.putInt("framesize",  cs.framesize);
  prefs.putInt("quality",    cs.quality);
  prefs.putInt("brightness", cs.brightness);
  prefs.putInt("contrast",   cs.contrast);
  prefs.putInt("saturation", cs.saturation);
  prefs.putInt("hmirror",    cs.hmirror);
  prefs.putInt("vflip",      cs.vflip);
  prefs.putInt("wb_mode",    cs.wb_mode);
  prefs.putInt("ae_level",   cs.ae_level);
  prefs.putInt("agc_gain",   cs.agc_gain);
  prefs.end();
  Serial.println("[cam] saved settings to NVS");
}

void camera_load_and_apply_settings() {
  Preferences prefs;
  if (!prefs.begin(NS_CAM, /*readOnly=*/true)) {
    // First-boot or NVS unavailable. Not an error — sensor keeps boot defaults.
    return;
  }
  CameraSettings cs;
  cs.framesize  = prefs.getInt("framesize",  -1);
  cs.quality    = prefs.getInt("quality",    -1);
  cs.brightness = prefs.getInt("brightness", -100);   // -100 = "not stored"
  cs.contrast   = prefs.getInt("contrast",   -100);
  cs.saturation = prefs.getInt("saturation", -100);
  cs.hmirror    = prefs.getInt("hmirror",    -1);
  cs.vflip      = prefs.getInt("vflip",      -1);
  cs.wb_mode    = prefs.getInt("wb_mode",    -1);
  cs.ae_level   = prefs.getInt("ae_level",   -100);
  cs.agc_gain   = prefs.getInt("agc_gain",   -1);
  prefs.end();

  if (camera_apply_settings(cs)) {
    Serial.println("[cam] applied saved settings from NVS");
  }
}

void camera_clear_saved_settings() {
  Preferences prefs;
  if (!prefs.begin(NS_CAM, /*readOnly=*/false)) {
    Serial.println("[cam] NVS open failed (clear)");
    return;
  }
  prefs.clear();
  prefs.end();
  Serial.println("[cam] cleared saved settings");
}

// Flash LED persistence is opt-in (the HTTP handler decides whether to
// save), so the slider can stream live updates during a drag without
// hammering NVS, only writing once on release.
void flash_led_save() {
  Preferences prefs;
  if (!prefs.begin(NS_FLASH, /*readOnly=*/false)) return;
  prefs.putUChar("duty", s_flash_duty);
  prefs.end();
}

void flash_led_load() {
  Preferences prefs;
  if (!prefs.begin(NS_FLASH, /*readOnly=*/true)) return;
  uint8_t duty = prefs.getUChar("duty", 0);
  prefs.end();
  if (duty != 0) {
    flash_led_set(duty);
    Serial.printf("[flash] restored duty=%u from NVS\n", duty);
  }
}
