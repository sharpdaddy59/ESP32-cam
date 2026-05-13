// usb_msc.cpp — present the SD card as a USB Mass Storage device.
//
// The ESP32-S3's native USB peripheral is wired to the USB-C connector on
// this board. We use the Arduino-ESP32 core's TinyUSB integration: USBMSC
// for the storage class, USB.onEvent() to detect enumeration vs VBUS-only.
//
// SD ownership:
//   - SD_MMC is always kept mounted (filesystem layer). MSC block-level
//     callbacks delegate to SD_MMC.readRAW() / writeRAW(), which operate
//     on sectors directly without involving the FAT layer.
//   - While MSC is active, firmware code MUST NOT write to the SD via the
//     filesystem — the host PC owns the FAT view of the volume. Reads are
//     also avoided because cached state can go stale.
//   - On USB unplug we tear down and re-mount SD_MMC to flush any caches,
//     then re-read /config/wifi.json so creds edited via MSC apply.

#include <Arduino.h>
#include <SD_MMC.h>
#include <USB.h>
#include <USBMSC.h>

#include "usb_msc.h"
#include "config.h"
#include "sd.h"
#include "net.h"

static USBMSC s_msc;
static volatile bool s_msc_active = false;

static int32_t on_msc_write(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
  // Single-sector at a time keeps the API simple; SD_MMC.writeRAW handles one
  // sector per call. bufsize should equal sectorSize() in practice.
  if (!SD_MMC.writeRAW(buffer, lba)) return 0;
  return bufsize;
}

static int32_t on_msc_read(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
  if (!SD_MMC.readRAW((uint8_t *)buffer, lba)) return 0;
  return bufsize;
}

static bool on_msc_start_stop(uint8_t power_condition, bool start, bool load_eject) {
  // Host requested eject/load. We accept silently — actual ownership swap
  // happens on the USB STARTED/STOPPED events, not here.
  return true;
}

static void on_usb_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
  if (base != ARDUINO_USB_EVENTS) return;

  switch (id) {
    case ARDUINO_USB_STARTED_EVENT:
      Serial.println("[msc] USB host enumerated → MSC active");
      s_msc_active = true;
      break;

    case ARDUINO_USB_STOPPED_EVENT:
      Serial.println("[msc] USB host disconnected → MSC inactive, re-mounting SD");
      s_msc_active = false;
      // Refresh the filesystem view so anything the host wrote is visible.
      sd_unmount();
      sd_mount();
      break;

    default:
      break;
  }
}

void usb_msc_init() {
  if (!sd_mounted()) {
    Serial.println("[msc] SD not mounted; MSC will be unavailable");
    return;
  }

  uint32_t sector_size = SD_MMC.sectorSize();
  uint32_t sector_count = SD_MMC.numSectors();
  Serial.printf("[msc] SD has %u sectors of %u bytes\n",
                (unsigned)sector_count, (unsigned)sector_size);

  s_msc.vendorID(USB_MANUFACTURER);
  s_msc.productID(USB_PRODUCT);
  s_msc.productRevision("1.0");
  s_msc.onRead(on_msc_read);
  s_msc.onWrite(on_msc_write);
  s_msc.onStartStop(on_msc_start_stop);
  s_msc.mediaPresent(true);
  s_msc.begin(sector_count, sector_size);

  USB.manufacturerName(USB_MANUFACTURER);
  USB.productName(USB_PRODUCT);
  USB.onEvent(on_usb_event);
  USB.begin();
}

bool msc_active() { return s_msc_active; }
