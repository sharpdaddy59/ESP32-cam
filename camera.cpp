// camera.cpp — esp_camera wrapper + flash-LED PWM.

#include <Arduino.h>
#include <esp_camera.h>

#include "camera.h"
#include "config.h"

static bool s_running = false;

bool camera_start() {
  if (s_running) return true;

  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_1;     // channel 0 is reserved for flash LED
  cfg.ledc_timer   = LEDC_TIMER_1;
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
  cfg.frame_size   = FRAMESIZE_UXGA;
  cfg.jpeg_quality = 10;
  cfg.fb_count     = 2;        // double-buffer for smoother streaming
  cfg.fb_location  = CAMERA_FB_IN_PSRAM;
  cfg.grab_mode    = CAMERA_GRAB_LATEST;

  if (!psramFound()) {
    Serial.println("[cam] WARN: PSRAM not found; falling back to SVGA single-buffer");
    cfg.frame_size  = FRAMESIZE_SVGA;
    cfg.fb_count    = 1;
    cfg.fb_location = CAMERA_FB_IN_DRAM;
    cfg.jpeg_quality = 12;
  }

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("[cam] init failed: 0x%x\n", err);
    return false;
  }

  // Conservative sensor defaults — most users want a "looks normal" image.
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
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
  }

  s_running = true;
  Serial.println("[cam] init OK");
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
  ledcSetup(FLASH_LED_CHANNEL, FLASH_LED_FREQ, FLASH_LED_RES_BITS);
  ledcAttachPin(FLASH_LED_PIN, FLASH_LED_CHANNEL);
  ledcWrite(FLASH_LED_CHANNEL, 0);
  s_flash_duty = 0;
}

void flash_led_set(uint8_t duty) {
  s_flash_duty = duty;
  ledcWrite(FLASH_LED_CHANNEL, duty);
}

uint8_t flash_led_get() { return s_flash_duty; }
