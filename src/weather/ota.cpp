#include "ota.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include "esp_ota_ops.h"
#include "config.h"
#include "net.h"
#include "store.h"

// A download keeps the radio on for about a minute, some thirty normal wakes' worth of battery. Two tries
// covers a dropped connection or one unlucky rollback; past that the version itself is the
// likely problem, and the board waits for the next release.
#define OTA_TRIES 2

// The Arduino core marks a new firmware as good the moment it starts, which would leave nothing
// to roll back to. Returning true here hands that decision to otaConfirm().
extern "C" bool verifyRollbackLater() { return true; }

static bool onProbation() {
  esp_ota_img_states_t state;
  return esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
         state == ESP_OTA_IMG_PENDING_VERIFY;
}

void otaConfirm() {
  if (!onProbation()) return;
  esp_ota_mark_app_valid_cancel_rollback();
  Serial.println("[ota] new firmware confirmed");
}

void otaUpdate(const String& weatherBody) {
  JsonDocument d;
  if (deserializeJson(d, weatherBody)) return;
  String version = d["ota_version"] | "";
  if (version.length() != 40) return;
  // A firmware still on probation installing another would leave neither slot known to work.
  if (onProbation()) return;

  // Counted before the download, so a try that hangs until someone resets the board still
  // counts, and so does a rollback: the old firmware comes back to this same offer.
  uint8_t tries = otaVersion() == version ? otaTries() : 0;
  if (tries >= OTA_TRIES) return;
  // The wake dropped Wi-Fi before drawing the screen. A connect that fails costs no try.
  if (!connectWiFi().ok) return;
  saveOtaTry(version, tries + 1);

  // Built here from SERVER_HOST rather than taken from the answer, so it can only ever point at
  // the server whose certificate the board checks.
  String url = "https://" SERVER_HOST "/firmware/" FW_BOARD "/" + version;
  Serial.printf("[ota] %s -> %.7s, try %u\n", FW_VERSION, version.c_str(), tries + 1);
  WiFiClientSecure client;
  trustServer(client);
  HTTPClient http;
  http.setTimeout(15000);
  if (!http.begin(client, url)) return;
  int code = http.GET();
  int size = http.getSize();
  if (code != 200 || size <= 0 || !Update.begin(size)) {
    Serial.printf("[ota] not started: http %d, %d bytes, %s\n", code, size, Update.errorString());
    http.end();
    return;
  }
  // end() checks the SHA-256 the build appends to every image before it switches slots, so a
  // download that arrived cut short or damaged is never booted.
  bool ok = Update.writeStream(*http.getStreamPtr()) == (size_t)size && Update.end();
  http.end();
  if (!ok) {
    Serial.printf("[ota] failed: %s\n", Update.errorString());
    Update.abort();
    return;
  }

  Serial.printf("[ota] installed %u ms into the wake, restarting\n", millis());
  clearWifiAttempts();              // this wake reached the network; the restart is no failed boot
  saveAwakeMs(millis());            // the next report carries how long the download kept it awake
  Serial.flush();
  ESP.restart();
}
