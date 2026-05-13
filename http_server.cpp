// http_server.cpp — AsyncWebServer routes + embedded HTML UI.
//
// MJPEG limit: this implementation uses module-level state to track the
// in-flight stream, which means only ONE viewer at a time. Opening /stream
// in a second tab will produce garbled output. The home page deliberately
// uses a single <img src="/stream"> tag to avoid this. The plan called out
// multi-viewer as nice-to-have, not required for MVP.

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <time.h>

#include "http_server.h"
#include "config.h"
#include "camera.h"
#include "sd.h"
#include "device_name.h"
#include "net.h"
#include "usb_msc.h"
#include "ota.h"

static AsyncWebServer server(HTTP_PORT);

// ---------------------------------------------------------------------------
// Embedded UI — minimal HTML/CSS/vanilla-JS, all in one self-contained page.
// ---------------------------------------------------------------------------
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<title>ESP32-S3-CAM</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
  :root { --fg:#222; --muted:#666; --accent:#0366d6; --bg:#f6f8fa; }
  body { font: 14px system-ui, sans-serif; color: var(--fg); background:#fff;
         max-width: 900px; margin: 1em auto; padding: 0 1em; }
  h1 { margin: 0 0 .3em; }
  .row { display:flex; gap:1em; flex-wrap:wrap; align-items:center; margin:.5em 0; }
  .stream { width:100%; max-width:800px; background:#000; aspect-ratio:4/3; }
  button { padding:.4em 1em; font:inherit; cursor:pointer;
           background:var(--accent); color:white; border:none; border-radius:4px; }
  button:hover { filter:brightness(1.1); }
  button.ghost { background:transparent; color:var(--accent); border:1px solid var(--accent); }
  details { margin:1em 0; padding:.6em 1em; background:var(--bg); border-radius:6px; }
  summary { cursor:pointer; font-weight:600; }
  .grid { display:grid; grid-template-columns: 9em 1fr 3em; gap:.4em .8em; align-items:center; margin-top:.5em; }
  .grid label { color:var(--muted); }
  .grid input[type=range] { width:100%; }
  .grid .val { font-variant-numeric: tabular-nums; text-align:right; color:var(--muted); }
  select { font:inherit; padding:.2em; }
  .pill { display:inline-block; padding:.1em .6em; background:var(--bg); border-radius:10px;
          color:var(--muted); margin-right:.4em; }
  .pill.warn { background:#fff8e1; color:#7a5a00; }
  .pill.err  { background:#ffebee; color:#a00; }
  .pill.ok   { background:#e8f5e9; color:#1b5e20; }
  table.status { border-collapse: collapse; width:100%; }
  table.status td { padding:.2em .6em; border-bottom:1px solid #eee; }
  table.status td:first-child { color:var(--muted); width:9em; }
  a { color:var(--accent); }
  code { background:var(--bg); padding:1px 4px; border-radius:3px; }
</style></head><body>
<h1>ESP32-S3-CAM</h1>
<p><span class="pill" id="hostnamePill">…</span><span class="pill" id="mscPill">…</span><span class="pill" id="wifiPill">…</span></p>
<img class="stream" id="stream" src="/stream" alt="live stream">
<div class="row">
  <button id="snap">Snapshot</button>
  <button class="ghost" id="reloadStream">Reload stream</button>
  <a href="/ota">Firmware update</a>
</div>

<details><summary>Camera settings</summary>
  <div class="grid" id="camGrid"></div>
  <div class="row" style="margin-top:.8em;">
    <button id="camSave">Apply</button>
  </div>
</details>

<details><summary>Flash LED</summary>
  <div class="grid">
    <label>Brightness</label>
    <input type="range" id="flashRange" min="0" max="255" value="0">
    <span class="val" id="flashVal">0</span>
  </div>
</details>

<details><summary>Status</summary>
  <table class="status" id="statusTable"></table>
  <p><button class="ghost" id="refreshStatus">Refresh</button></p>
</details>

<details><summary>SD card</summary>
  <div class="row"><input id="sdPath" value="/" style="flex:1; font:inherit;"> <button class="ghost" id="sdLs">List</button></div>
  <pre id="sdOut" style="background:var(--bg); padding:.6em; border-radius:4px; overflow:auto; max-height:20em;"></pre>
</details>

<details><summary>Network</summary>
  <p>Hostname: <code id="hostname">…</code> (default <code id="hostnameDefault">…</code>)</p>
  <div class="row">
    <input id="hostnameNew" placeholder="new hostname (a-z, 0-9, -)" style="flex:1; font:inherit;">
    <button class="ghost" id="hostnameSet">Set</button>
  </div>
  <p style="margin-top:1em;"><button class="ghost" id="wifiReset">Forget Wi-Fi creds and reboot</button></p>
</details>

<script>
const $ = sel => document.querySelector(sel);
const fmt = (n, unit) => n == null ? '—' : (Math.round(n*10)/10) + (unit||'');

// Snapshot — open the JPEG in a new tab.
$('#snap').onclick = () => window.open('/snapshot?t=' + Date.now(), '_blank');

// Reload stream by re-setting src (cache-bust).
$('#reloadStream').onclick = () => {
  const img = $('#stream');
  img.src = '/stream?t=' + Date.now();
};

// Camera settings — schema-driven grid.
const CAM_SCHEMA = [
  { key:'framesize',  label:'Resolution', type:'select', options:[
    [13,'UXGA 1600x1200'],[12,'SXGA 1280x1024'],[10,'XGA 1024x768'],
    [9,'SVGA 800x600'],[8,'VGA 640x480'],[6,'QVGA 320x240']
  ]},
  { key:'quality',    label:'JPEG quality', type:'range', min:4, max:63, step:1 },
  { key:'brightness', label:'Brightness',   type:'range', min:-2, max:2, step:1 },
  { key:'contrast',   label:'Contrast',     type:'range', min:-2, max:2, step:1 },
  { key:'saturation', label:'Saturation',   type:'range', min:-2, max:2, step:1 },
  { key:'ae_level',   label:'AE level',     type:'range', min:-2, max:2, step:1 },
  { key:'agc_gain',   label:'AGC gain',     type:'range', min:0, max:30, step:1 },
  { key:'wb_mode',    label:'White balance',type:'select', options:[
    [0,'auto'],[1,'sunny'],[2,'cloudy'],[3,'office'],[4,'home']
  ]},
  { key:'hmirror',    label:'H-mirror',     type:'check' },
  { key:'vflip',      label:'V-flip',       type:'check' },
];

function renderCam(values) {
  const g = $('#camGrid');
  g.innerHTML = '';
  for (const f of CAM_SCHEMA) {
    const label = document.createElement('label'); label.textContent = f.label;
    let input, val;
    if (f.type === 'range') {
      input = document.createElement('input'); input.type='range';
      input.min=f.min; input.max=f.max; input.step=f.step;
      input.value = values[f.key] ?? 0; input.dataset.key = f.key;
      val = document.createElement('span'); val.className='val'; val.textContent=input.value;
      input.oninput = () => val.textContent = input.value;
    } else if (f.type === 'select') {
      input = document.createElement('select'); input.dataset.key = f.key;
      for (const [v,n] of f.options) {
        const o = document.createElement('option'); o.value=v; o.textContent=n;
        if (Number(values[f.key]) === v) o.selected = true;
        input.appendChild(o);
      }
      val = document.createElement('span');
    } else { // check
      input = document.createElement('input'); input.type='checkbox';
      input.checked = !!values[f.key]; input.dataset.key = f.key;
      val = document.createElement('span');
    }
    g.appendChild(label); g.appendChild(input); g.appendChild(val);
  }
}

async function loadCam() {
  const r = await fetch('/camera'); const j = await r.json(); renderCam(j);
}

$('#camSave').onclick = async () => {
  const body = {};
  for (const el of $('#camGrid').querySelectorAll('[data-key]')) {
    if (el.type === 'checkbox') body[el.dataset.key] = el.checked ? 1 : 0;
    else body[el.dataset.key] = Number(el.value);
  }
  await fetch('/camera', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(body) });
  // Reload stream so the new settings are visible immediately.
  $('#stream').src = '/stream?t=' + Date.now();
};

// Flash LED
const flashRange = $('#flashRange'), flashVal = $('#flashVal');
async function loadFlash() {
  const r = await fetch('/flash'); const j = await r.json();
  flashRange.value = j.duty; flashVal.textContent = j.duty;
}
flashRange.oninput = async () => {
  flashVal.textContent = flashRange.value;
  await fetch('/flash', { method:'POST', headers:{'Content-Type':'application/json'},
                          body: JSON.stringify({ duty: Number(flashRange.value) }) });
};

// Status
async function loadStatus() {
  const r = await fetch('/status'); const j = await r.json();
  const rows = [
    ['Firmware', j.firmware],
    ['Uptime', j.uptime_s + ' s'],
    ['Free heap', j.heap_free + ' B'],
    ['Free PSRAM', j.psram_free + ' B'],
    ['Wi-Fi SSID', j.ssid || '(disconnected)'],
    ['Wi-Fi RSSI', j.rssi + ' dBm'],
    ['IP', j.ip],
    ['Hostname', j.hostname],
    ['SD total', (j.sd_total/(1024*1024)).toFixed(0) + ' MB'],
    ['SD free',  (j.sd_free /(1024*1024)).toFixed(0) + ' MB'],
    ['MSC active', j.msc_active ? 'yes' : 'no'],
    ['Camera', j.camera_running ? 'running' : 'stopped'],
  ];
  $('#statusTable').innerHTML = rows.map(r =>
    `<tr><td>${r[0]}</td><td>${r[1]}</td></tr>`).join('');
  $('#hostnamePill').textContent = j.hostname + '.local';
  $('#mscPill').textContent = j.msc_active ? 'USB mode' : 'standalone';
  $('#mscPill').className = 'pill ' + (j.msc_active ? 'warn' : 'ok');
  $('#wifiPill').textContent = j.ssid ? `${j.ssid} ${j.rssi}dBm` : 'no wifi';
  $('#wifiPill').className = 'pill ' + (j.ssid ? 'ok' : 'err');
  $('#hostname').textContent = j.hostname;
}
$('#refreshStatus').onclick = loadStatus;

// SD listing
$('#sdLs').onclick = async () => {
  const path = encodeURIComponent($('#sdPath').value || '/');
  const r = await fetch('/sd/list?path=' + path); const j = await r.json();
  $('#sdOut').textContent = JSON.stringify(j, null, 2);
};

// Hostname
$('#hostnameSet').onclick = async () => {
  const name = $('#hostnameNew').value.trim();
  const r = await fetch('/hostname', { method:'POST',
    headers:{'Content-Type':'application/json'}, body: JSON.stringify({ name }) });
  if (r.ok) { await loadStatus(); $('#hostnameNew').value = ''; }
  else alert('Failed: ' + await r.text());
};
$('#wifiReset').onclick = async () => {
  if (!confirm('Delete wifi.json and reboot?')) return;
  await fetch('/wifi/reset', { method:'POST' });
};

// Boot
loadCam(); loadFlash(); loadStatus();
setInterval(loadStatus, 5000);
</script>
</body></html>)HTML";

// ---------------------------------------------------------------------------
// /stream — MJPEG multipart. Module-level state, single viewer.
// ---------------------------------------------------------------------------
static camera_fb_t *s_stream_fb = nullptr;
static size_t       s_stream_pos = 0;
static int          s_stream_phase = 0;  // 0=need-header, 1=sending-jpeg
static String       s_stream_header;
static volatile bool s_stream_busy = false;

static size_t stream_filler(uint8_t *buffer, size_t maxLen, size_t index) {
  size_t written = 0;
  while (written < maxLen) {
    if (s_stream_phase == 0) {
      // Need a fresh frame and its header.
      if (!s_stream_fb) {
        s_stream_fb = camera_grab();
        if (!s_stream_fb) {
          // Camera grab failed; signal a short read so AsyncWebServer retries soon.
          return written;
        }
        s_stream_pos = 0;
        s_stream_header  = "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ";
        s_stream_header += s_stream_fb->len;
        s_stream_header += "\r\n\r\n";
      }
      size_t remaining = s_stream_header.length() - s_stream_pos;
      size_t to_send   = remaining < (maxLen - written) ? remaining : (maxLen - written);
      memcpy(buffer + written, s_stream_header.c_str() + s_stream_pos, to_send);
      written       += to_send;
      s_stream_pos  += to_send;
      if (s_stream_pos == s_stream_header.length()) {
        s_stream_phase = 1;
        s_stream_pos = 0;
      }
    } else {
      size_t remaining = s_stream_fb->len - s_stream_pos;
      size_t to_send   = remaining < (maxLen - written) ? remaining : (maxLen - written);
      memcpy(buffer + written, s_stream_fb->buf + s_stream_pos, to_send);
      written       += to_send;
      s_stream_pos  += to_send;
      if (s_stream_pos == s_stream_fb->len) {
        esp_camera_fb_return(s_stream_fb);
        s_stream_fb = nullptr;
        s_stream_phase = 0;
        s_stream_pos = 0;
      }
    }
  }
  return written;
}

static void on_stream(AsyncWebServerRequest *request) {
  if (s_stream_busy) {
    request->send(503, "text/plain", "another viewer is streaming");
    return;
  }
  s_stream_busy = true;
  // Reset state for this connection.
  if (s_stream_fb) {
    esp_camera_fb_return(s_stream_fb);
    s_stream_fb = nullptr;
  }
  s_stream_phase = 0;
  s_stream_pos = 0;

  AsyncWebServerResponse *response = request->beginChunkedResponse(
    "multipart/x-mixed-replace;boundary=frame", stream_filler);
  response->addHeader("Cache-Control", "no-store");
  response->addHeader("Connection", "close");

  request->onDisconnect([]() {
    if (s_stream_fb) {
      esp_camera_fb_return(s_stream_fb);
      s_stream_fb = nullptr;
    }
    s_stream_phase = 0;
    s_stream_pos = 0;
    s_stream_busy = false;
    Serial.println("[http] stream disconnected");
  });

  request->send(response);
}

// ---------------------------------------------------------------------------
// /snapshot — single JPEG. Holds the previous fb until the next request to
// avoid a use-after-free racing AsyncWebServer's response send.
// ---------------------------------------------------------------------------
static camera_fb_t *s_last_snap_fb = nullptr;

static void on_snapshot(AsyncWebServerRequest *request) {
  if (s_last_snap_fb) {
    esp_camera_fb_return(s_last_snap_fb);
    s_last_snap_fb = nullptr;
  }

  camera_fb_t *fb = camera_grab();
  if (!fb) {
    request->send(500, "text/plain", "camera grab failed");
    return;
  }

  if (!msc_active() && sd_mounted()) {
    sd_save_snapshot(fb->buf, fb->len);
  }

  AsyncWebServerResponse *r = request->beginResponse_P(200, "image/jpeg", fb->buf, fb->len);
  r->addHeader("Cache-Control", "no-store");
  r->addHeader("Content-Disposition", "inline; filename=snapshot.jpg");
  request->send(r);

  s_last_snap_fb = fb;  // released on next request
}

// ---------------------------------------------------------------------------
// /status
// ---------------------------------------------------------------------------
static void on_status(AsyncWebServerRequest *request) {
  JsonDocument doc;
  doc["firmware"]       = FW_VERSION;
  doc["uptime_s"]       = (uint32_t)(millis() / 1000);
  doc["heap_free"]      = (uint32_t)ESP.getFreeHeap();
  doc["psram_free"]     = (uint32_t)ESP.getFreePsram();
  doc["ssid"]           = WiFi.SSID();
  doc["rssi"]           = net_is_connected() ? WiFi.RSSI() : 0;
  doc["ip"]             = WiFi.localIP().toString();
  doc["hostname"]       = device_hostname();
  doc["sd_total"]       = (uint32_t)(sd_total_bytes() / 1024);   // KB
  doc["sd_free"]        = (uint32_t)(sd_free_bytes()  / 1024);
  doc["msc_active"]     = msc_active();
  doc["camera_running"] = camera_running();

  String body; serializeJson(doc, body);
  request->send(200, "application/json", body);
}

// ---------------------------------------------------------------------------
// /camera — GET returns settings, POST updates them
// ---------------------------------------------------------------------------
static void on_camera_get(AsyncWebServerRequest *request) {
  CameraSettings cs = camera_get_settings();
  JsonDocument doc;
  doc["framesize"]  = cs.framesize;
  doc["quality"]    = cs.quality;
  doc["brightness"] = cs.brightness;
  doc["contrast"]   = cs.contrast;
  doc["saturation"] = cs.saturation;
  doc["hmirror"]    = cs.hmirror;
  doc["vflip"]      = cs.vflip;
  doc["wb_mode"]    = cs.wb_mode;
  doc["ae_level"]   = cs.ae_level;
  doc["agc_gain"]   = cs.agc_gain;
  String body; serializeJson(doc, body);
  request->send(200, "application/json", body);
}

// JSON body handler shared by /camera POST and /flash POST.
static void on_camera_post_body(AsyncWebServerRequest *request, uint8_t *data,
                                size_t len, size_t index, size_t total) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    request->send(400, "text/plain", err.c_str());
    return;
  }
  CameraSettings cs;
  cs.framesize  = doc["framesize"]  | -1;
  cs.quality    = doc["quality"]    | -1;
  cs.brightness = doc["brightness"] | -100;
  cs.contrast   = doc["contrast"]   | -100;
  cs.saturation = doc["saturation"] | -100;
  cs.hmirror    = doc["hmirror"]    | -1;
  cs.vflip      = doc["vflip"]      | -1;
  cs.wb_mode    = doc["wb_mode"]    | -1;
  cs.ae_level   = doc["ae_level"]   | -100;
  cs.agc_gain   = doc["agc_gain"]   | -1;
  camera_apply_settings(cs);
  on_camera_get(request);
}

// ---------------------------------------------------------------------------
// /flash
// ---------------------------------------------------------------------------
static void on_flash_get(AsyncWebServerRequest *request) {
  JsonDocument doc;
  doc["duty"] = flash_led_get();
  String body; serializeJson(doc, body);
  request->send(200, "application/json", body);
}

static void on_flash_post_body(AsyncWebServerRequest *request, uint8_t *data,
                               size_t len, size_t index, size_t total) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) {
    request->send(400, "text/plain", "bad json");
    return;
  }
  int duty = doc["duty"] | -1;
  if (duty < 0 || duty > 255) {
    request->send(400, "text/plain", "duty must be 0..255");
    return;
  }
  flash_led_set((uint8_t)duty);
  on_flash_get(request);
}

// ---------------------------------------------------------------------------
// /sd/list and /sd/get
// ---------------------------------------------------------------------------
static void on_sd_list(AsyncWebServerRequest *request) {
  if (msc_active()) {
    request->send(503, "text/plain", "SD owned by USB host");
    return;
  }
  if (!sd_mounted()) {
    request->send(503, "text/plain", "SD not mounted");
    return;
  }
  String path = "/";
  if (request->hasParam("path")) path = request->getParam("path")->value();
  request->send(200, "application/json", sd_list_json(path.c_str()));
}

static const char *guess_mime(const String &path) {
  String lp = path; lp.toLowerCase();
  if (lp.endsWith(".jpg") || lp.endsWith(".jpeg")) return "image/jpeg";
  if (lp.endsWith(".png"))  return "image/png";
  if (lp.endsWith(".json")) return "application/json";
  if (lp.endsWith(".txt"))  return "text/plain";
  if (lp.endsWith(".html") || lp.endsWith(".htm")) return "text/html";
  if (lp.endsWith(".css"))  return "text/css";
  if (lp.endsWith(".js"))   return "application/javascript";
  return "application/octet-stream";
}

static void on_sd_get(AsyncWebServerRequest *request) {
  if (msc_active()) {
    request->send(503, "text/plain", "SD owned by USB host");
    return;
  }
  if (!sd_mounted()) {
    request->send(503, "text/plain", "SD not mounted");
    return;
  }
  if (!request->hasParam("path")) {
    request->send(400, "text/plain", "missing path");
    return;
  }
  String path = request->getParam("path")->value();
  if (!SD_MMC.exists(path)) {
    request->send(404, "text/plain", "not found");
    return;
  }
  request->send(SD_MMC, path, guess_mime(path));
}

// ---------------------------------------------------------------------------
// /wifi/reset, /hostname
// ---------------------------------------------------------------------------
static void on_wifi_reset(AsyncWebServerRequest *request) {
  request->send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
  delay(200);
  net_reset_credentials();
}

static void on_hostname_get(AsyncWebServerRequest *request) {
  JsonDocument doc;
  doc["current"] = device_hostname();
  doc["default"] = device_hostname_default();
  doc["mac"]     = device_mac();
  String body; serializeJson(doc, body);
  request->send(200, "application/json", body);
}

static void on_hostname_post_body(AsyncWebServerRequest *request, uint8_t *data,
                                  size_t len, size_t index, size_t total) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) {
    request->send(400, "text/plain", "bad json");
    return;
  }
  const char *name = doc["name"] | "";
  if (!device_hostname_set(name)) {
    request->send(400, "text/plain", "invalid name");
    return;
  }
  net_apply_hostname_change();
  on_hostname_get(request);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void http_server_begin() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/stream",   HTTP_GET,  on_stream);
  server.on("/snapshot", HTTP_GET,  on_snapshot);
  server.on("/status",   HTTP_GET,  on_status);

  server.on("/camera",   HTTP_GET,  on_camera_get);
  server.on("/camera",   HTTP_POST,
            [](AsyncWebServerRequest *r){}, nullptr, on_camera_post_body);

  server.on("/flash",    HTTP_GET,  on_flash_get);
  server.on("/flash",    HTTP_POST,
            [](AsyncWebServerRequest *r){}, nullptr, on_flash_post_body);

  server.on("/sd/list",  HTTP_GET,  on_sd_list);
  server.on("/sd/get",   HTTP_GET,  on_sd_get);

  server.on("/wifi/reset", HTTP_POST, on_wifi_reset);
  server.on("/hostname",   HTTP_GET,  on_hostname_get);
  server.on("/hostname",   HTTP_POST,
            [](AsyncWebServerRequest *r){}, nullptr, on_hostname_post_body);

  ota_register(server);

  server.onNotFound([](AsyncWebServerRequest *r) {
    r->send(404, "text/plain", "not found");
  });

  server.begin();
  Serial.printf("[http] listening on port %d\n", HTTP_PORT);
}
