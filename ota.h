// ota.h — HTTP-based OTA firmware update on AsyncWebServer.
//
// Adds:
//   GET  /ota          — minimal HTML page with a file picker and progress UI
//   POST /ota/upload   — multipart upload of a raw .bin firmware image
//
// Architecture differs from cores3-hydro's sync-WebServer version:
// AsyncWebServer's upload handler is callback-based and runs on its own
// task. We stream directly to Update.write() rather than buffering the
// whole image in PSRAM, because AsyncWebServer's request body is
// re-entrant friendly.

#pragma once

class AsyncWebServer;

void ota_register(AsyncWebServer &server);
