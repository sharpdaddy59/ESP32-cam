// http_server.h — AsyncWebServer with all routes and the embedded HTML UI.
//
// Routes registered:
//   GET  /                — home page (live stream, sliders, snapshot button)
//   GET  /stream          — MJPEG multipart stream
//   GET  /snapshot        — single JPEG (also saved to SD)
//   GET  /status          — JSON system status
//   GET  /camera          — current camera settings as JSON
//   POST /camera          — update camera settings (any subset)
//   GET  /flash           — current flash LED duty
//   POST /flash           — set flash LED duty
//   GET  /sd/list?path=   — directory listing JSON
//   GET  /sd/get?path=    — file contents
//   POST /wifi/reset      — delete creds and reboot
//   GET  /hostname        — current hostname + default + MAC
//   POST /hostname        — set hostname override
//   /ota, /ota/upload     — registered by ota_register()

#pragma once

void http_server_begin();
