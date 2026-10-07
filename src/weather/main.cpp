#include <Arduino.h>
#include <ArduinoJson.h>
#include "esp_sleep.h"
#include "config.h"
#include "net.h"
#include "battery.h"
#include "room.h"
#include "provision.h"
#include "store.h"
#include "display.h"

// What this wake knows about itself. Filled as the values become available — battery and chip
// before the radio, since both change once it is on — and read by everything downstream, so the
// helpers below take one of these instead of their own selection of the same four numbers.
struct Wake {
  uint32_t   batteryMv;     // resting voltage, comparable across wakes
  float      chipC;         // die temperature, close to the room before the radio warms it
  Room       room;          // the actual room, if the sensor is fitted
  uint8_t    attempts;      // boots since the last success; 1 means none were missed
  uint8_t    resetReason;   // esp_reset_reason(). 8 is the timer, anything else is worth seeing
  WifiResult wifi;
};

// The fields that describe the wake itself. They go on the report and on every log entry alike,
// so the names live here once rather than being spelled out on both paths.
static void addWakeFacts(JsonDocument& d, const Wake& w) {
  d["battery_mv"]    = w.batteryMv;
  d["wifi_attempts"] = w.attempts;
  d["reset_reason"]  = w.resetReason;
}

// Files a wake the server never heard about, to ride along on the next report that gets through.
// No timestamp: the board has no clock, so the server stamps the report carrying these and counts
// forward with the sleep constants. That is why reset_reason is on every entry — a crash or a
// brownout comes back immediately rather than after RETRY_MINUTES, and nothing else says so.
// The caller fills in whatever is particular to this kind of failure before calling.
static void logWake(const char* kind, const Wake& w, JsonDocument& e) {
  e["kind"] = kind;
  addWakeFacts(e, w);
  String entry;
  serializeJson(e, entry);
  appendLog(entry);
}

// Puts the setup page on the air, and never returns: the board either restarts with a network
// or sleeps with no wake timer until someone presses RESET. firstRun only decides what the
// screen says — the board has never been set up, or the network it knows stopped answering.
static void enterSetupMode(bool firstRun, const Wake& w) {
  JsonDocument opened;
  logWake(firstRun ? "portal-new" : "portal-lost", w, opened);

  displayBegin();
  displayMessage(firstRun ? "Wi-Fi setup" : "Wi-Fi not found",
                "1. Connect your phone\n"
                "   to the Wi-Fi network\n"
                "   " AP_NAME "\n"
                "\n"
                "2. Open in a browser\n"
                "   192.168.4.1");

  // Whatever is stored stays stored, so a portal nobody answers still leaves the board able to
  // reconnect by itself once the router comes back.
  bool saved = runSetupPortal(AP_NAME, PORTAL_MINUTES * 60);

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
  logWake("portal-timeout", w, timedOut);

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
static String wakeReport(const Wake& w) {
  JsonDocument req;
  // Which board this came from. Without it the server has one stream of readings from however
  // many boards are pointed at it, and anything that compares a row with the one before it —
  // the battery curve, the wake time — is comparing two different devices.
  req["mac"]           = readMacAddress();
  addWakeFacts(req, w);
  req["wifi_ms"]       = w.wifi.ms;
  req["rssi"]          = w.wifi.rssi;
  req["fw"]            = FW_VERSION;
  // A cell holds less charge when it is cold, so without this the battery curve mixes the
  // weather in with the discharge and neither can be read off it.
  req["chip_c"]        = roundf(w.chipC * 10) / 10;
  // Left out entirely when the sensor is missing, rather than sent as zero — a board without one
  // should read as "no sensor", not as a freezing room.
  if (w.room.ok) {
    req["room_c"]  = roundf(w.room.c * 10) / 10;
    req["room_rh"] = roundf(w.room.rh * 10) / 10;
  }
  // Sent on every wake rather than only when true, so its presence also marks a build that knows
  // how to clear the bus. The firmware stamp cannot do that: it is the commit plus a dirty flag,
  // so two different working trees on the same commit report the same string.
  req["room_stuck"]    = w.room.stuck;
  // The previous wake's, not this one's — see store.h. Left out on the first wake after a fresh
  // NVS, where there is no previous one: zero would read as a wake that took no time at all.
  if (uint32_t awake = lastAwakeMs()) req["prev_awake_ms"] = awake;
  req["nvs_free"]      = nvsFreeEntries();
  req["nvs_total"]     = nvsTotalEntries();
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
  Serial.println("fw " FW_VERSION);           // a -dirty suffix means this build matches no commit
  Wake w;
  // Both before Wi-Fi: a resting voltage, and a die that has not warmed itself up on the radio
  // yet, so the reading is close to the room. Comparable across wakes because it is always here.
  w.batteryMv   = batteryMillivolts();
  w.chipC       = temperatureRead();
  w.room        = readRoom();
  w.resetReason = esp_reset_reason();
  Serial.printf("battery %u mV, chip %.1f C", w.batteryMv, w.chipC);
  if (w.room.ok) Serial.printf(", room %.1f C %.1f%%", w.room.c, w.room.rh);
  Serial.println();

  // Two ways to need the portal: the board has never been set up, or the network it was set up
  // for has stopped answering. Nothing below runs until that is resolved — nothing to fetch.
  w.attempts     = recordWifiAttempt();
  bool  firstRun = !wifiProvisioned();
  if (firstRun || w.attempts >= WIFI_FAIL_LIMIT) enterSetupMode(firstRun, w);

  // connectWiFi() brings the radio up and disconnectWifi() puts it down, so this brackets the whole
  // window that draws ~100 mA — the figure the battery pays, of which the connect is only part.
  uint32_t radioOnAt = millis();
  w.wifi = connectWiFi();

  // How far the wake gets decides what to store and how soon to come back. Reaching the network
  // is all the portal counter tracks — a server being down is no reason to ask someone to set up
  // Wi-Fi again.
  bool    updated  = false;
  uint8_t nextWake = RETRY_MINUTES;
  if (w.wifi.ok) {
    HttpResult http = httpPost(WEATHER_URL, wakeReport(w));
    if (http.code == 200) {
      saveWeather(http.body);
      clearLog();                 // only a 200 retires the log: anything else may not have landed
      updated  = true;
      nextWake = SLEEP_MINUTES;
      Serial.println("[weather updated]\n" + http.body);
    } else {
      JsonDocument e;
      e["wifi_ms"]   = w.wifi.ms;
      e["rssi"]      = w.wifi.rssi;
      e["http_code"] = http.code;
      logWake("http", w, e);
      Serial.println("fetch failed — redraw stored weather");
    }
  } else {
    JsonDocument e;
    e["wifi_ms"]     = w.wifi.ms;   // near the timeout means it waited it out, far under means it
    e["wifi_status"] = w.wifi.status;   // gave up early on a status that was already final
    logWake("wifi", w, e);
    Serial.println("Wi-Fi failed — redraw stored weather");
  }
  disconnectWifi();
  Serial.printf("radio on for %u ms\n", millis() - radioOnAt);

  // NVS is the single source of truth, so the screen draws what is stored whether or not this
  // wake added to it. Redraw every time, so the status line reflects THIS wake.
  displayBegin();
  displayWeather(lastWeather(), updated, w.wifi.rssi, w.batteryMv, w.room);

  // Cleared here rather than the moment the connect succeeds, which is 0.5 s into a 4.4 s wake.
  // Five resets in a row is how someone asks for the setup page, and hitting a half-second
  // window five times is not something anyone can do — the whole wake is the window now. It
  // still means the same thing, only written later: a boot that got interrupted before the
  // screen was drawn should count, the same as one that never reached the network.
  if (w.wifi.ok) clearWifiAttempts();
  sleepUntilNextWake(nextWake);
}

void loop() {
  // A deep-sleep wake is a full reset -> execution restarts from setup(), so loop() is unused
  //uint32_t batteryMv = batteryMillivolts();
  //Serial.printf("battery %u mV\n", batteryMv);
  //delay(1000);
}
