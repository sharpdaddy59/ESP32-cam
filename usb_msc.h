// usb_msc.h — present the SD card as a USB Mass Storage device.
//
// Built on the Arduino-ESP32 core's TinyUSB integration (USB.h + USBMSC.h).
// Detection gates on USB *enumeration* (a real host on the data lines), not
// VBUS — wall chargers and power banks leave us in standalone mode.
//
// Coordination with the firmware:
//   - On STARTED event: sd_unmount(), then USBMSC.begin() with SD as backing.
//   - On STOPPED event: USBMSC.end(), sd_mount(), re-read wifi.json so any
//     edits the user just made apply immediately.
//   - While msc_active() returns true, SD-touching endpoints in
//     http_server.cpp short-circuit with a 503-style message.

#pragma once

#include <Arduino.h>

void usb_msc_init();
bool msc_active();
