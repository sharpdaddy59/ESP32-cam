// ota.cpp — AsyncWebServer-flavored HTTP firmware upload.
//
// Streams the upload directly into the Update partition rather than
// buffering in PSRAM. AsyncWebServer's upload callback is invoked in
// fixed-size chunks; we write each chunk to Update as it arrives. If
// anything fails mid-stream the partition isn't switched, and a power
// loss leaves the running firmware intact.

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>

#include "ota.h"
#include "config.h"

static bool s_failed = false;
static String s_error_msg;

static void on_upload_chunk(AsyncWebServerRequest *request,
                            const String &filename, size_t index,
                            uint8_t *data, size_t len, bool final_chunk) {
  if (index == 0) {
    Serial.printf("[ota] upload start: filename=\"%s\"\n", filename.c_str());
    s_failed    = false;
    s_error_msg = "";
    // UPDATE_SIZE_UNKNOWN: stream-mode. Update will compute size as bytes
    // flow in. Partition selection is automatic.
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      s_failed    = true;
      s_error_msg = String("Update.begin: ") + Update.errorString();
      Serial.printf("[ota] %s\n", s_error_msg.c_str());
      return;
    }
  }

  if (s_failed) return;

  if (len > 0) {
    size_t written = Update.write(data, len);
    if (written != len) {
      s_failed    = true;
      s_error_msg = String("Update.write short: ") + (uint32_t)written +
                    "/" + (uint32_t)len;
      Serial.printf("[ota] %s\n", s_error_msg.c_str());
      return;
    }
  }

  if (final_chunk) {
    if (!Update.end(true)) {
      s_failed    = true;
      s_error_msg = String("Update.end: ") + Update.errorString();
      Serial.printf("[ota] %s\n", s_error_msg.c_str());
      return;
    }
    Serial.printf("[ota] upload complete: %u bytes; rebooting\n",
                  (unsigned)(index + len));
  }
}

static void on_upload_complete(AsyncWebServerRequest *request) {
  if (s_failed) {
    String body = "{\"ok\":false,\"error\":\"";
    String esc = s_error_msg;
    esc.replace("\"", "'");
    body += esc;
    body += "\"}";
    request->send(500, "application/json", body);
    return;
  }
  static const char *kDone =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<title>OTA OK</title>"
    "<meta http-equiv=\"refresh\" content=\"15;url=/\">"
    "<style>body{font:15px system-ui,sans-serif;max-width:560px;"
    "margin:2em auto;padding:0 1em;}</style></head><body>"
    "<h1>Update OK</h1>"
    "<p>Device is rebooting. This page will redirect home in 15 seconds.</p>"
    "</body></html>";
  request->send(200, "text/html", kDone);
  delay(500);
  ESP.restart();
}

static void on_ota_index(AsyncWebServerRequest *request) {
  static const char *kPage =
    "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
    "<title>OTA</title>"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<style>"
    "body{font:15px system-ui,sans-serif;max-width:560px;margin:2em auto;padding:0 1em;color:#222}"
    "h1{margin:0 0 .5em}"
    "form{margin:1.5em 0;padding:1em;border:1px solid #ddd;border-radius:6px}"
    "button{padding:.5em 1em;font:inherit;cursor:pointer;background:#0366d6;color:white;border:none;border-radius:4px}"
    "button:disabled{background:#999;cursor:not-allowed}"
    "progress{width:100%;height:1.5em}"
    ".status{margin-top:1em;min-height:1.2em;color:#666}"
    ".status.err{color:#c00}"
    "a{color:#0366d6}"
    "code{background:#eee;padding:1px 4px;border-radius:3px}"
    "</style></head><body>"
    "<h1>Firmware update</h1>"
    "<p>Current firmware: <code>" FW_VERSION "</code></p>"
    "<form id=\"f\" enctype=\"multipart/form-data\" method=\"POST\" action=\"/ota/upload\">"
    "<p><input id=\"file\" type=\"file\" name=\"firmware\" accept=\".bin\" required></p>"
    "<p><button id=\"go\" type=\"submit\">Upload firmware</button></p>"
    "<progress id=\"p\" value=\"0\" max=\"100\" hidden></progress>"
    "<div id=\"s\" class=\"status\"></div>"
    "</form>"
    "<p><a href=\"/\">&larr; back to home</a></p>"
    "<script>"
    "const f=document.getElementById('f'),p=document.getElementById('p'),"
    "s=document.getElementById('s'),go=document.getElementById('go');"
    "f.addEventListener('submit',e=>{e.preventDefault();const fd=new FormData(f),x=new XMLHttpRequest();"
    "x.upload.addEventListener('progress',ev=>{if(ev.lengthComputable){p.hidden=false;p.max=ev.total;p.value=ev.loaded;"
    "s.className='status';s.textContent='Uploading '+Math.round(ev.loaded/ev.total*100)+'%...'}});"
    "x.addEventListener('load',()=>{if(x.status>=200&&x.status<300){s.className='status';"
    "s.innerHTML='Upload complete. Device rebooting &mdash; <a href=\"/\">return home</a>.'}"
    "else{s.className='status err';s.textContent='Failed ('+x.status+'): '+x.responseText;go.disabled=false}});"
    "x.addEventListener('error',()=>{s.className='status err';s.textContent='Connection error.';go.disabled=false});"
    "go.disabled=true;s.className='status';s.textContent='Starting upload...';"
    "x.open('POST','/ota/upload');x.send(fd)});"
    "</script></body></html>";
  request->send(200, "text/html", kPage);
}

void ota_register(AsyncWebServer &server) {
  server.on("/ota", HTTP_GET, on_ota_index);
  server.on("/ota/upload", HTTP_POST,
            on_upload_complete,
            on_upload_chunk);
}
