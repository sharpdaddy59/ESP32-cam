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
#include <img_converters.h>
#include <time.h>

#include "http_server.h"
#include "config.h"
#include "camera.h"
#include "sd.h"
#include "device_name.h"
#include "net.h"
#include "ota.h"
#include "motion.h"

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
<h1>ESP32-CAM</h1>
<p><span class="pill" id="hostnamePill">…</span><span class="pill" id="modePill">…</span><span class="pill" id="sdPill">…</span><span class="pill" id="wifiPill">…</span></p>
<div class="row" id="modeBar">
  <button data-mode="0" class="ghost">Idle</button>
  <button data-mode="1" class="ghost">Stream</button>
  <button data-mode="2" class="ghost">Motion</button>
</div>
<img class="stream" id="stream" alt="(loading...)">
<div id="viewerNote" style="text-align:center; color:var(--muted); margin-top:-.4em; min-height:1.2em;"></div>
<div class="row">
  <button id="snap">Snapshot</button>
  <button class="ghost" id="reloadStream">Reload</button>
  <a href="/ota">Firmware update</a>
</div>

<details><summary>Camera settings</summary>
  <div class="grid" id="camGrid"></div>
  <div class="row" style="margin-top:.8em;">
    <button id="camSave">Apply</button>
    <button class="ghost" id="camReset">Reset to defaults</button>
  </div>
</details>

<details><summary>Flash LED</summary>
  <div class="grid">
    <label>Brightness</label>
    <input type="range" id="flashRange" min="0" max="255" value="0">
    <span class="val" id="flashVal">0</span>
  </div>
</details>

<details><summary>Motion detection</summary>
  <p style="color:var(--muted); margin:.4em 0;">Switch the mode at the top to <b>Motion</b> to activate. Lower sensitivity = trigger on smaller changes. Cooldown suppresses repeat triggers for N seconds after each.</p>
  <div class="grid">
    <label>Sensitivity</label>
    <input type="range" id="motionThreshold" min="1" max="50" step="1">
    <span class="val" id="motionThresholdVal">—</span>
    <label>Cooldown (s)</label>
    <input type="range" id="motionCooldown" min="1" max="600" step="1">
    <span class="val" id="motionCooldownVal">—</span>
    <label>Webhook URL</label>
    <input type="text" id="motionWebhook" placeholder="https://… (optional)" style="font:inherit; padding:.2em; min-width:0;">
    <span></span>
  </div>
  <div class="row" style="margin-top:.8em">
    <button id="motionSave">Save</button>
  </div>
  <table class="status" id="motionStatusTbl" style="margin-top:1em;"></table>
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

// Reload current view (whatever the mode is).
$('#reloadStream').onclick = () => applyMode(currentMode);

// Mode handling -----------------------------------------------------------
let currentMode = 0;
function applyMode(mode) {
  document.querySelectorAll('[data-mode]').forEach(b => {
    b.className = (Number(b.dataset.mode) === mode) ? '' : 'ghost';
  });
  const names = ['idle','stream','motion'];
  $('#modePill').textContent = names[mode] || '?';
  $('#modePill').className = 'pill ' + (mode === 0 ? 'warn' : 'ok');
  const img = $('#stream');
  const note = $('#viewerNote');
  if (mode === 1) {
    img.src = '/stream?t=' + Date.now();
    img.style.opacity = '1';
    note.textContent = '';
  } else if (mode === 2) {
    img.src = '/motion/last.jpg?t=' + Date.now();
    img.style.opacity = '1';
    note.textContent = 'Most recent triggered frame (or blank if no trigger yet)';
  } else {
    img.removeAttribute('src');
    img.style.opacity = '.15';
    note.textContent = 'Idle — live view disabled. Snapshot still works.';
  }
}
async function loadMode() {
  const j = await (await fetch('/mode')).json();
  currentMode = j.mode;
  applyMode(j.mode);
}
document.querySelectorAll('[data-mode]').forEach(b => {
  b.onclick = async () => {
    const m = Number(b.dataset.mode);
    if (m === currentMode) return;
    document.querySelectorAll('[data-mode]').forEach(x => x.disabled = true);
    try {
      await fetch('/mode', { method:'POST',
        headers:{'Content-Type':'application/json'},
        body: JSON.stringify({mode: m}) });
      // Camera reconfig takes ~1 s in/out of motion mode.
      await new Promise(r => setTimeout(r, 1500));
      await loadMode();
    } finally {
      document.querySelectorAll('[data-mode]').forEach(x => x.disabled = false);
    }
  };
});

// Motion config -----------------------------------------------------------
async function loadMotion() {
  const c = await (await fetch('/motion/config')).json();
  $('#motionThreshold').value = c.threshold;
  $('#motionThresholdVal').textContent = c.threshold;
  $('#motionCooldown').value = c.cooldown_s;
  $('#motionCooldownVal').textContent = c.cooldown_s;
  $('#motionWebhook').value = c.webhook || '';
}
$('#motionThreshold').oninput = () => $('#motionThresholdVal').textContent = $('#motionThreshold').value;
$('#motionCooldown').oninput  = () => $('#motionCooldownVal').textContent  = $('#motionCooldown').value;
$('#motionSave').onclick = async () => {
  await fetch('/motion/config', { method:'POST',
    headers:{'Content-Type':'application/json'},
    body: JSON.stringify({
      threshold:  Number($('#motionThreshold').value),
      cooldown_s: Number($('#motionCooldown').value),
      webhook:    $('#motionWebhook').value
    }) });
};
async function loadMotionStatus() {
  const s = await (await fetch('/motion/status')).json();
  const ts = s.last_trigger_time
    ? new Date(s.last_trigger_time*1000).toLocaleString()
    : 'never';
  const rows = [
    ['Mode', s.mode_name],
    ['Last trigger', ts],
    ['Triggers total', s.triggers_total],
    ['Current diff', s.sad + ' / threshold ' + s.threshold],
  ];
  $('#motionStatusTbl').innerHTML = rows.map(r =>
    `<tr><td>${r[0]}</td><td>${r[1]}</td></tr>`).join('');
  // Auto-refresh the trigger image while in motion mode.
  if (s.mode === 2 && s.last_trigger_time > 0) {
    $('#stream').src = '/motion/last.jpg?t=' + Date.now();
  }
}
setInterval(loadMotionStatus, 5000);

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

$('#camReset').onclick = async () => {
  if (!confirm('Clear saved camera settings and reboot to defaults?')) return;
  await fetch('/camera/reset', { method:'POST' });
};

// Flash LED
const flashRange = $('#flashRange'), flashVal = $('#flashVal');
async function loadFlash() {
  const r = await fetch('/flash'); const j = await r.json();
  flashRange.value = j.duty; flashVal.textContent = j.duty;
}
// oninput fires continuously while dragging — send live updates without
// the save=true side-effect so we don't thrash NVS at every slider tick.
flashRange.oninput = () => {
  flashVal.textContent = flashRange.value;
  fetch('/flash', { method:'POST', headers:{'Content-Type':'application/json'},
                    body: JSON.stringify({ duty: Number(flashRange.value), save: false }) });
};
// onchange fires once when the slider is released — that's when we commit.
flashRange.onchange = () => {
  fetch('/flash', { method:'POST', headers:{'Content-Type':'application/json'},
                    body: JSON.stringify({ duty: Number(flashRange.value), save: true }) });
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
    ['SD mounted', j.sd_mounted ? 'yes' : 'no'],
    ['Camera', j.camera_running ? 'running' : 'stopped'],
    ['CPU temp', (j.cpu_temp_c != null ? j.cpu_temp_c.toFixed(1) : '—') + ' °C'],
  ];
  $('#statusTable').innerHTML = rows.map(r =>
    `<tr><td>${r[0]}</td><td>${r[1]}</td></tr>`).join('');
  $('#hostnamePill').textContent = j.hostname + '.local';
  $('#sdPill').textContent = j.sd_mounted ? `SD ${Math.round(j.sd_free/1024)}MB free` : 'no SD';
  $('#sdPill').className = 'pill ' + (j.sd_mounted ? 'ok' : 'warn');
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
loadMode(); loadCam(); loadFlash(); loadStatus(); loadMotion(); loadMotionStatus();
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
  // Bail cleanly on mode change so motion_set_mode's camera reconfig can
  // proceed without racing our held s_stream_fb. We do NOT call fb_return
  // on s_stream_fb here — motion_set_mode invalidates it via
  // http_server_invalidate_stream_state() before deiniting the camera, at
  // which point the pointer is already gone.
  if (motion_get_mode() != MODE_STREAM) {
    if (s_stream_fb) {
      esp_camera_fb_return(s_stream_fb);
      s_stream_fb = nullptr;
    }
    return 0;
  }

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
  if (motion_get_mode() != MODE_STREAM) {
    request->send(503, "text/plain", "stream disabled in current mode");
    return;
  }
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
// /snapshot — single JPEG.
//
// Copies the JPEG bytes into a PSRAM heap buffer and releases the camera fb
// immediately, so the live stream and the camera driver never compete with
// us for buffers. The heap copy is freed via onDisconnect (forced prompt by
// Connection: close).
// ---------------------------------------------------------------------------
static void on_snapshot(AsyncWebServerRequest *request) {
  camera_fb_t *fb = camera_grab();
  if (!fb) {
    request->send(500, "text/plain", "camera grab failed");
    return;
  }

  // In motion mode the camera is configured in GRAYSCALE — encode to JPEG.
  // In stream/idle mode the camera output is already JPEG; just copy.
  uint8_t *buf = nullptr;
  size_t   len = 0;
  if (fb->format == PIXFORMAT_JPEG) {
    len = fb->len;
    buf = (uint8_t *)ps_malloc(len);
    if (!buf) {
      esp_camera_fb_return(fb);
      request->send(500, "text/plain", "out of PSRAM for snapshot");
      return;
    }
    memcpy(buf, fb->buf, len);
  } else {
    // GRAYSCALE → encode. frame2jpg() allocates with malloc(); free path
    // below is unified since malloc/ps_malloc share the same heap-free.
    if (!frame2jpg(fb, 80, &buf, &len) || !buf || len == 0) {
      esp_camera_fb_return(fb);
      request->send(500, "text/plain", "frame2jpg failed");
      return;
    }
  }

  // Return fb to the camera pool as soon as buf has the bytes — keeps the
  // pool from starving the stream/motion task during a slow SD write below.
  esp_camera_fb_return(fb);

  if (sd_mounted()) {
    sd_save_snapshot(buf, len);
  }

  AsyncWebServerResponse *r = request->beginResponse(200, "image/jpeg", buf, len);
  r->addHeader("Cache-Control", "no-store");
  r->addHeader("Content-Disposition", "inline; filename=snapshot.jpg");
  // Force Connection: close so onDisconnect fires promptly after the response
  // is sent (otherwise keep-alive could hold the buffer alive indefinitely).
  r->addHeader("Connection", "close");
  request->onDisconnect([buf]() { free(buf); });
  request->send(r);
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
  doc["sd_mounted"]     = sd_mounted();
  doc["camera_running"] = camera_running();
  // Classic ESP32 internal temp sensor — uncalibrated, self-heats with CPU
  // load. Useful as a relative indicator (e.g. "+15 °C under streaming
  // load") rather than an absolute room-temperature reading.
  doc["cpu_temp_c"]     = temperatureRead();
  doc["mode"]           = (int)motion_get_mode();

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
  camera_save_settings();   // persist to NVS so the next boot keeps them
  on_camera_get(request);
}

// POST /camera/reset — clear saved overrides and reboot so camera_start()'s
// defaults take effect on a clean slate.
static void on_camera_reset(AsyncWebServerRequest *request) {
  camera_clear_saved_settings();
  request->send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
  delay(200);
  ESP.restart();
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
  // Optional "save" key gates the NVS write. Defaults to true so curl /
  // scripted callers get persistence by default; the UI sends save=false
  // during slider drag and save=true on release.
  bool save = doc["save"] | true;
  flash_led_set((uint8_t)duty);
  if (save) flash_led_save();
  on_flash_get(request);
}

// ---------------------------------------------------------------------------
// /sd/list and /sd/get
// ---------------------------------------------------------------------------
static void on_sd_list(AsyncWebServerRequest *request) {
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
// /mode and /motion/*
// ---------------------------------------------------------------------------
static const char *mode_name(Mode m) {
  switch (m) {
    case MODE_IDLE:   return "idle";
    case MODE_STREAM: return "stream";
    case MODE_MOTION: return "motion";
  }
  return "?";
}

static void on_mode_get(AsyncWebServerRequest *request) {
  Mode m = motion_get_mode();
  JsonDocument doc;
  doc["mode"]      = (int)m;
  doc["mode_name"] = mode_name(m);
  String body; serializeJson(doc, body);
  request->send(200, "application/json", body);
}

static void on_mode_post_body(AsyncWebServerRequest *request, uint8_t *data,
                              size_t len, size_t index, size_t total) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) {
    request->send(400, "text/plain", "bad json");
    return;
  }
  int m = -1;
  if (doc["mode"].is<int>()) {
    m = doc["mode"].as<int>();
  } else if (doc["mode"].is<const char *>()) {
    String s = doc["mode"].as<const char *>();
    if      (s == "idle")   m = MODE_IDLE;
    else if (s == "stream") m = MODE_STREAM;
    else if (s == "motion") m = MODE_MOTION;
  }
  if (m < MODE_IDLE || m > MODE_MOTION) {
    request->send(400, "text/plain", "mode must be 0/1/2 or idle/stream/motion");
    return;
  }
  if (!motion_set_mode((Mode)m)) {
    request->send(500, "text/plain", "mode change failed (camera reconfig)");
    return;
  }
  on_mode_get(request);
}

static void on_motion_config_get(AsyncWebServerRequest *request) {
  JsonDocument doc;
  doc["threshold"]  = motion_get_threshold();
  doc["cooldown_s"] = motion_get_cooldown_s();
  doc["webhook"]    = motion_get_webhook();
  String body; serializeJson(doc, body);
  request->send(200, "application/json", body);
}

static void on_motion_config_post_body(AsyncWebServerRequest *request, uint8_t *data,
                                       size_t len, size_t index, size_t total) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) {
    request->send(400, "text/plain", "bad json");
    return;
  }
  if (doc["threshold"].is<int>()) {
    int t = doc["threshold"].as<int>();
    if (t < 0 || t > 255) {
      request->send(400, "text/plain", "threshold must be 0..255");
      return;
    }
    motion_set_threshold((uint8_t)t);
  }
  if (doc["cooldown_s"].is<int>()) {
    motion_set_cooldown_s(doc["cooldown_s"].as<int>());
  }
  if (doc["webhook"].is<const char *>()) {
    motion_set_webhook(String(doc["webhook"].as<const char *>()));
  }
  on_motion_config_get(request);
}

static void on_motion_status(AsyncWebServerRequest *request) {
  JsonDocument doc;
  Mode m = motion_get_mode();
  doc["mode"]              = (int)m;
  doc["mode_name"]         = mode_name(m);
  doc["sad"]               = motion_last_sad();
  doc["threshold"]         = motion_get_threshold();
  doc["last_trigger_time"] = motion_last_trigger_time();
  doc["triggers_total"]    = motion_trigger_count();
  String body; serializeJson(doc, body);
  request->send(200, "application/json", body);
}

static void on_motion_last_jpg(AsyncWebServerRequest *request) {
  uint8_t *buf = nullptr;
  size_t   len = 0;
  if (!motion_copy_last_jpeg(&buf, &len)) {
    request->send(404, "text/plain", "no trigger yet");
    return;
  }
  AsyncWebServerResponse *r = request->beginResponse(200, "image/jpeg", buf, len);
  r->addHeader("Cache-Control", "no-store");
  r->addHeader("Connection", "close");
  request->onDisconnect([buf]() { free(buf); });
  request->send(r);
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void http_server_begin() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  server.on("/stream",   HTTP_GET,  on_stream);
  server.on("/snapshot", HTTP_GET,  on_snapshot);
  server.on("/status",   HTTP_GET,  on_status);

  server.on("/camera",       HTTP_GET,  on_camera_get);
  server.on("/camera",       HTTP_POST,
            [](AsyncWebServerRequest *r){}, nullptr, on_camera_post_body);
  server.on("/camera/reset", HTTP_POST, on_camera_reset);

  server.on("/flash",    HTTP_GET,  on_flash_get);
  server.on("/flash",    HTTP_POST,
            [](AsyncWebServerRequest *r){}, nullptr, on_flash_post_body);

  server.on("/sd/list",  HTTP_GET,  on_sd_list);
  server.on("/sd/get",   HTTP_GET,  on_sd_get);

  server.on("/wifi/reset", HTTP_POST, on_wifi_reset);
  server.on("/hostname",   HTTP_GET,  on_hostname_get);
  server.on("/hostname",   HTTP_POST,
            [](AsyncWebServerRequest *r){}, nullptr, on_hostname_post_body);

  server.on("/mode",            HTTP_GET,  on_mode_get);
  server.on("/mode",            HTTP_POST,
            [](AsyncWebServerRequest *r){}, nullptr, on_mode_post_body);
  server.on("/motion/config",   HTTP_GET,  on_motion_config_get);
  server.on("/motion/config",   HTTP_POST,
            [](AsyncWebServerRequest *r){}, nullptr, on_motion_config_post_body);
  server.on("/motion/status",   HTTP_GET,  on_motion_status);
  server.on("/motion/last.jpg", HTTP_GET,  on_motion_last_jpg);

  ota_register(server);

  server.onNotFound([](AsyncWebServerRequest *r) {
    r->send(404, "text/plain", "not found");
  });

  server.begin();
  Serial.printf("[http] listening on port %d\n", HTTP_PORT);
}

void http_server_invalidate_stream_state() {
  // The held fb is about to be freed underneath us by camera_deinit. Drop
  // the pointer without calling fb_return; the underlying buffer goes away
  // with the camera driver. s_stream_busy stays true — the active stream's
  // next filler call will hit the mode-mismatch branch above and exit
  // cleanly (which clears s_stream_busy via the onDisconnect callback).
  s_stream_fb    = nullptr;
  s_stream_phase = 0;
  s_stream_pos   = 0;
}
