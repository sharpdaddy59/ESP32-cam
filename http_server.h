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

// Drop any in-flight stream's held camera_fb_t pointer WITHOUT calling
// esp_camera_fb_return on it. Used by motion_set_mode() right before it
// deinits the camera — the pointer is about to become invalid and a stale
// fb_return would crash. The stream's active TCP response will exit cleanly
// on its next filler invocation via the mode-check at the top.
void http_server_invalidate_stream_state();
