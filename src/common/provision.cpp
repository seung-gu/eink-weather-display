#include "provision.h"
#include <WiFi.h>
#include <WiFiManager.h>

bool wifiProvisioned() {
  WiFi.mode(WIFI_STA);            // the config is only readable once the interface exists
  wifi_config_t cfg;
  return esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK && cfg.sta.ssid[0] != '\0';
}

bool runProvisioningPortal(const char* apName, uint16_t timeoutSec) {
  WiFiManager wm;
  wm.setConfigPortalTimeout(timeoutSec);
  // The portal tests the credentials before accepting them, so a wrong password leaves the
  // page open for another try rather than returning here.
  // Both default to keeping the portal alive — a joined station, or a page load, pushes the
  // start time forward and the timeout never arrives. An access point costs ~100 mA, so the
  // limit has to be an absolute wall clock, not a measure of idleness.
  wm.setAPClientCheck(false);
  wm.setWebPortalClientCheck(false);
  return wm.autoConnect(apName);
}

bool runRecoveryPortal(const char* apName, uint16_t timeoutSec) {
  WiFiManager wm;
  wm.setConfigPortalTimeout(timeoutSec);
  wm.setAPClientCheck(true);
  // startConfigPortal, not autoConnect: it serves the page without first trying (and failing)
  // to reach the saved network, and it leaves that network in place if nobody saves anything.
  return wm.startConfigPortal(apName);
}
