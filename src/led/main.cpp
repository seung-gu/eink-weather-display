#include <Arduino.h>
#include "config.h"
#include "net.h"

// Onboard LED on the ESP32-C3 Super Mini: GPIO8, active-LOW (LED_PIN from config.h)
static void setLed(bool on) { digitalWrite(LED_PIN, on ? LOW : HIGH); }

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  setLed(false);
  connectWiFi();
}

void loop() {
  if (!wifiConnected()) connectWiFi();

  String led = httpGet(LED_URL);            // server returns "1" (on) or "0" (off)
  if (led.length()) setLed(led[0] == '1');

  delay(LED_POLL_MS);
}
