// wifi_setup.cpp — load Wi-Fi creds from SD.

#include "wifi_setup.h"
#include "sd.h"

bool wifi_creds_load(WifiCreds &out) {
  return sd_load_wifi_creds(out);
}

bool wifi_creds_clear() {
  return sd_clear_wifi_creds();
}
