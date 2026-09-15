#include <Arduino.h>
#include "esp_sleep.h"
#include "config.h"
#include "net.h"
#include "battery.h"
#include "provision.h"
#include "store.h"
#include "display.h"

// Sleep this long, then wake and refresh the weather (shorten while testing)
#define SLEEP_MINUTES 10

// Puts the setup page on the air, and never returns: the board either restarts with a network
// or sleeps with no wake timer until someone presses RESET.
static void runSetupPortal(bool firstRun) {
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
  displayMessage("Setup timed out", "Press RESET to set up\nWi-Fi again.");
  Serial.flush();
  esp_deep_sleep_start();                     // no wake timer: asleep until someone resets it
}

static void sleepUntilNextWake() {
#ifndef DEBUG_NO_SLEEP
  // On timer expiry the chip resets and restarts from setup()
  Serial.printf("deep sleep for %d min...\n", SLEEP_MINUTES);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_MINUTES * 60 * 1000000ULL);
  esp_deep_sleep_start();
#else
  Serial.println("[debug] staying awake (no deep sleep)");
#endif
}

void setup() {
  Serial.begin(115200);
  uint32_t batteryMv = batteryMillivolts();   // before Wi-Fi: a resting voltage, comparable across wakes
  Serial.printf("battery %u mV\n", batteryMv);

  // Two ways to need the portal: the board has never been set up, or the network it was set up
  // for has stopped answering. Nothing below runs until that is resolved — nothing to fetch.
  uint8_t fails    = recordWifiAttempt();
  bool    firstRun = !wifiProvisioned();
  if (firstRun || fails >= WIFI_FAIL_LIMIT) runSetupPortal(firstRun);

  WifiResult wifi = connectWiFi();

  String fetched;
  if (wifi.ok) {
    // This wake's state rides along on the weather request, so it costs no extra round trip.
    String report = String(batteryMv) + "," + String(wifi.ms) + "," + String(wifi.rssi);
    fetched = httpPost(WEATHER_URL, report);         // retries once inside
  }
  wifiOff();                                         // wifi.ms/rssi are already captured

  // NVS is the single source of truth: store what is fresh, then draw what is stored.
  if (fetched.length()) saveWeather(fetched);
  String w = lastWeather();
  if (wifi.ok) clearWifiAttempts();   // reaching the network is what clears the count

  Serial.println(fetched.length() ? "[weather updated]\n" + w
                                  : (wifi.ok ? "fetch failed — redraw stored weather"
                                  : "Wi-Fi failed — redraw stored weather"));

  // Always redraw, so the status line reflects THIS wake.
  displayBegin();
  displayWeather(w, fetched.length() > 0, wifi.ok ? wifi.rssi : 0, batteryMv);

  sleepUntilNextWake();
}

void loop() {
  // A deep-sleep wake is a full reset -> execution restarts from setup(), so loop() is unused
  //uint32_t batteryMv = batteryMillivolts();
  //Serial.printf("battery %u mV\n", batteryMv);
  //delay(1000);
}
