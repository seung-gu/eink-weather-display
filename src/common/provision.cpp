#include "provision.h"
#include <WiFi.h>
#include <WiFiManager.h>

bool wifiProvisioned() {
  WiFi.mode(WIFI_STA);            // the config is only readable once the interface exists
  wifi_config_t cfg;
  return esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK && cfg.sta.ssid[0] != '\0';
}

bool runSetupPortal(const char* apName, uint16_t timeoutSec) {
  WiFiManager wm;
  wm.setConfigPortalTimeout(timeoutSec);

  // Both restart the timeout by default — one when a phone is joined, the other when it loads
  // the page. An access point costs ~100 mA, so the limit has to be a plain wall clock.
  wm.setAPClientCheck(false);
  wm.setWebPortalClientCheck(false);

  // 1. open access point named apName, reachable at 192.168.4.1 — the same address the screen
  //    tells people to open, in case the phone does not offer the page by itself
  // 2. DNS server answering every query with that address, so whatever page the phone opens
  //    lands on the setup form
  // 3. web server serving that form
  // 4. blocks here until it is filled in or the timeout runs out
  //
  // true  credentials saved — the portal tested them before accepting
  // false the timeout ran out. A wrong password does not end it: the page stays open for
  //       another try, so ten wrong tries inside the window still return true.
  //
  // startConfigPortal, not autoConnect: autoConnect tries the stored network first, and on ESP32
  // it does that even with nothing stored, which is a 60 second wait with the radio on.
  return wm.startConfigPortal(apName);
}
