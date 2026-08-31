#include <Arduino.h>
#include <Preferences.h>
#include "esp_sleep.h"
#include "config.h"
#include "net.h"
#include "display.h"

// Sleep this long, then wake and refresh the weather (shorten while testing)
#define SLEEP_MINUTES 10

// Last good response, kept in NVS so it survives deep sleep, resets and power loss.
static Preferences prefs;

void setup() {
  Serial.begin(115200);

  WifiResult wifi = connectWiFi();

  String fetched;
  if (wifi.ok) fetched = httpGet(WEATHER_URL);       // retries once inside
  wifiOff();                                         // wifi.ms/rssi are already captured

  // NVS is the single source of truth: store what is fresh, then draw what is stored.
  prefs.begin("weather", false);
  if (fetched.length()) prefs.putString("last", fetched);
  String w = prefs.getString("last", "");
  prefs.end();

  Serial.println(fetched.length() ? "[weather updated]\n" + w
                                  : (wifi.ok ? "fetch failed — redraw stored weather"
                                             : "Wi-Fi failed — redraw stored weather"));

  // Always redraw, so the status line reflects THIS wake. 0 = offline.
  displayBegin();
  displayWeather(w, wifi.ok ? wifi.ms : 0, wifi.ok ? wifi.rssi : 0);

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

void loop() {
  // A deep-sleep wake is a full reset -> execution restarts from setup(), so loop() is unused
}
