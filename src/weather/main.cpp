#include <Arduino.h>
#include <Preferences.h>
#include "esp_sleep.h"
#include "config.h"
#include "net.h"
#include "display.h"

// Sleep this long, then wake and refresh the weather (shorten while testing)
#define SLEEP_MINUTES 10

// Last good weather response, kept in NVS (flash) so it survives deep sleep, resets and
// power loss — lets us redraw the screen when a fetch fails instead of losing it.
// NVS skips the write when the value is unchanged, so flash wear stays negligible.
static Preferences prefs;


void setup() {
  Serial.begin(115200);

  // Wi-Fi -> weather -> screen. On any failure, skip the rest and just deep sleep
  // (retries on the next wake). No restart loop -> battery-safe.
  WifiResult wifi = connectWiFi();

  String fetched;
  if (wifi.ok) fetched = httpGet(WEATHER_URL);       // retries once inside

  // NVS is the single source of truth: store a fresh response, then always draw what's stored.
  // A failed fetch then simply redraws the last good data, with no separate fallback path.
  prefs.begin("weather", false);
  if (fetched.length()) prefs.putString("last", fetched);
  String w = prefs.getString("last", "");
  prefs.end();

  Serial.println(fetched.length() ? "[weather updated]\n" + w
                                  : (wifi.ok ? "fetch failed — redraw stored weather"
                                             : "Wi-Fi failed — redraw stored weather"));

  // Always redraw (full refresh) so the status line reflects THIS wake, even with no
  // weather data at all (empty w just leaves the weather area blank). 0 = offline.
  displayBegin();
  displayWeather(w, wifi.ok ? wifi.ms : 0, wifi.ok ? wifi.rssi : 0);

#ifndef DEBUG_NO_SLEEP
  // Enter deep sleep -> on timer expiry the chip resets and restarts from setup()
  Serial.printf("deep sleep for %d min...\n", SLEEP_MINUTES);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_MINUTES * 60 * 1000000ULL);
  esp_deep_sleep_start();
#else
  Serial.println("[debug] staying awake (no deep sleep)");   // 디버그 빌드에선 안 잠
#endif
}

void loop() {
  // A deep-sleep wake is a full reset -> execution restarts from setup(), so loop() is unused
}
