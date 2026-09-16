#include <Arduino.h>
#include <ArduinoJson.h>
#include "esp_sleep.h"
#include "config.h"
#include "net.h"
#include "battery.h"
#include "provision.h"
#include "store.h"
#include "display.h"

// Files a wake the server never heard about, to ride along on the next report that gets through.
// No timestamp: the board has no clock, so the server stamps the report carrying these and counts
// forward with the sleep constants. That is why reset_reason is on every entry — a crash or a
// brownout comes back immediately rather than after RETRY_MINUTES, and nothing else says so.
//
// batteryMv is always the reading from the top of setup(), taken before the radio came up. Read
// again under the portal's ~100 mA and the number stops comparing with the other entries.
static void logWake(const char* kind, uint8_t attempts, uint32_t batteryMv, JsonDocument& e) {
  e["kind"]          = kind;
  e["wifi_attempts"] = attempts;
  e["reset_reason"]  = (int)esp_reset_reason();
  e["battery_mv"]    = batteryMv;
  String entry;
  serializeJson(e, entry);
  appendLog(entry);
}

// Puts the setup page on the air, and never returns: the board either restarts with a network
// or sleeps with no wake timer until someone presses RESET.
static void runSetupPortal(bool firstRun, uint8_t fails, uint32_t batteryMv) {
  JsonDocument opened;
  logWake(firstRun ? "portal-new" : "portal-lost", fails, batteryMv, opened);

  displayBegin();
  displayMessage(firstRun ? "Wi-Fi setup" : "Wi-Fi not found",
                "1. Connect your phone\n"
                "   to the Wi-Fi network\n"
                "   " AP_NAME "\n"
                "\n"
                "2. Open in a browser\n"
                "   192.168.4.1");

  // A first run has nothing to preserve; a recovery keeps the saved network, so the board can
  // reconnect by itself if the router simply came back.
  bool saved = firstRun ? runProvisioningPortal(AP_NAME, PORTAL_MINUTES * 60)
                        : runRecoveryPortal(AP_NAME, PORTAL_MINUTES * 60);

  clearWifiAttempts();
  if (saved) ESP.restart();                   // clean boot: STA only, portal memory released

  // Nobody came. Sleeping on the timer would mean another access point every WIFI_FAIL_LIMIT
  // wakes, and an access point costs ~100 mA — that empties the battery in a day.
  //
  // This entry closes the timeline. Everything before it sits on a fixed schedule the server can
  // count through; past it the board is asleep with no timer, and how long it stays that way is
  // up to whoever walks over and presses RESET. Without the marker the server keeps counting and
  // reports a confident, wrong time.
  JsonDocument timedOut;
  logWake("portal-timeout", fails, batteryMv, timedOut);

  displayMessage("Setup timed out", "Press RESET to set up\nWi-Fi again.");
  saveAwakeMs(millis());            // minutes of access point at ~100 mA — the costliest wake there is
  Serial.flush();
  esp_deep_sleep_start();                     // no wake timer: asleep until someone resets it
}

static void sleepUntilNextWake(uint8_t minutes) {
  // Last thing before sleeping, so this covers the whole wake — the screen refresh included,
  // which is seconds of it. The next wake reports it; this one's report left long ago.
  saveAwakeMs(millis());
#ifndef DEBUG_NO_SLEEP
  // On timer expiry the chip resets and restarts from setup()
  Serial.printf("deep sleep for %u min...\n", minutes);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)minutes * 60 * 1000000ULL);
  esp_deep_sleep_start();
#else
  Serial.println("[debug] staying awake (no deep sleep)");
#endif
}

// This wake's state rides along on the weather request, so it costs no extra round trip — and so
// do the wakes that never got to send one. reset_reason and wifi_attempts are what let the server
// place those in time: it stamps this report and counts backwards through the sleep constants,
// and a gap in wifi_attempts tells it a boot went by without even managing to leave an entry.
static String wakeReport(uint32_t batteryMv, float chipC, const WifiResult& wifi,
                         uint8_t attempts) {
  JsonDocument req;
  req["battery_mv"]    = batteryMv;
  req["wifi_ms"]       = wifi.ms;
  req["rssi"]          = wifi.rssi;
  req["reset_reason"]  = (int)esp_reset_reason();
  req["wifi_attempts"] = attempts;
  req["fw"]            = FW_VERSION;
  // A cell holds less charge when it is cold, so without this the battery curve mixes the
  // weather in with the discharge and neither can be read off it.
  req["chip_c"]        = roundf(chipC * 10) / 10;
  // The previous wake's, not this one's — see store.h. Wakes are alike enough that the figure
  // still pairs with the battery reading beside it.
  req["prev_awake_ms"] = lastAwakeMs();
  req["nvs_free"]      = nvsFreeEntries();
  // serialized() drops the stored text in as JSON rather than quoting it into a string, so the
  // array crosses the wire without being parsed here and taken apart again at the far end.
  String backlog = lastLog();
  if (backlog.length()) req["log"] = serialized(backlog);
  String body;
  serializeJson(req, body);
  return body;
}

void setup() {
  Serial.begin(115200);
  // Both before Wi-Fi: a resting voltage, and a die that has not warmed itself up on the radio
  // yet, so the reading is close to the room. Comparable across wakes because it is always here.
  uint32_t batteryMv = batteryMillivolts();
  float    chipC     = temperatureRead();
  Serial.printf("battery %u mV, chip %.1f C\n", batteryMv, chipC);

  // Two ways to need the portal: the board has never been set up, or the network it was set up
  // for has stopped answering. Nothing below runs until that is resolved — nothing to fetch.
  uint8_t fails    = recordWifiAttempt();
  bool    firstRun = !wifiProvisioned();
  if (firstRun || fails >= WIFI_FAIL_LIMIT) runSetupPortal(firstRun, fails, batteryMv);

  // connectWiFi() brings the radio up and wifiOff() puts it down, so this brackets the whole
  // window that draws ~100 mA — the figure the battery pays, of which the connect is only part.
  uint32_t radioOnAt = millis();
  WifiResult wifi = connectWiFi();

  // How far the wake gets decides what to store and how soon to come back. Reaching the network
  // is all the portal counter tracks — a server being down is no reason to ask someone to set up
  // Wi-Fi again.
  bool    updated  = false;
  uint8_t nextWake = RETRY_MINUTES;
  if (wifi.ok) {
    clearWifiAttempts();
    HttpResult http = httpPost(WEATHER_URL, wakeReport(batteryMv, chipC, wifi, fails));
    if (http.code == 200) {
      saveWeather(http.body);
      clearLog();                 // only a 200 retires the log: anything else may not have landed
      updated  = true;
      nextWake = SLEEP_MINUTES;
      Serial.println("[weather updated]\n" + http.body);
    } else {
      JsonDocument e;
      e["wifi_ms"]   = wifi.ms;
      e["rssi"]      = wifi.rssi;
      e["http_code"] = http.code;
      logWake("http", fails, batteryMv, e);
      Serial.println("fetch failed — redraw stored weather");
    }
  } else {
    JsonDocument e;
    e["wifi_ms"]     = wifi.ms;   // near the timeout means it waited it out, far under means it
    e["wifi_status"] = wifi.status;   // gave up early on a status that was already final
    logWake("wifi", fails, batteryMv, e);
    Serial.println("Wi-Fi failed — redraw stored weather");
  }
  wifiOff();
  Serial.printf("radio on for %u ms\n", millis() - radioOnAt);

  // NVS is the single source of truth, so the screen draws what is stored whether or not this
  // wake added to it. Redraw every time, so the status line reflects THIS wake.
  displayBegin();
  displayWeather(lastWeather(), updated, wifi.rssi, batteryMv);
  sleepUntilNextWake(nextWake);
}

void loop() {
  // A deep-sleep wake is a full reset -> execution restarts from setup(), so loop() is unused
  //uint32_t batteryMv = batteryMillivolts();
  //Serial.printf("battery %u mV\n", batteryMv);
  //delay(1000);
}
