#include <Arduino.h>
#include "esp_sleep.h"
#include "config.h"
#include "net.h"
#include "display.h"

// Sleep this long, then wake and refresh the weather (shorten while testing)
#define SLEEP_MINUTES 10

// --- LED control (currently unused, commented out) ---
// static void setLed(bool on) { digitalWrite(LED_PIN, on ? LOW : HIGH); }

void setup() {
  Serial.begin(115200);

  // --- Onboard LED (currently commented out) ---
  // pinMode(LED_PIN, OUTPUT);
  // setLed(false);

  uint32_t wifiMs = connectWiFi();
  displayBegin();

  // Fetch weather and refresh the screen (e-Paper holds the image with no power)
  String w = httpGet(WEATHER_URL);
  if (w.length()) {
    displayWeather(w, wifiMs);
    Serial.println("[weather updated]\n" + w);
  }

  // --- Reflect server LED state (currently commented out) ---
  // String led = httpGet(LED_URL);
  // if (led.length()) setLed(led[0] == '1');

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
