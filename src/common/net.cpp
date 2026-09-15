#include "net.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "config.h"

// When the radio came on, so wifiOff() can report the whole window.
static uint32_t radioOnAt = 0;

// WiFi.waitForConnectResult() polls every 100 ms, which leaves the radio on for up to that long
// after the association is already done — and rounds every measurement to 100 ms. Same logic,
// ten times finer. Failure codes still bail early instead of waiting out the timeout.
static bool waitConnected(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    wl_status_t s = WiFi.status();
    if (s == WL_CONNECTED) return true;
    if (s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL) return false;
    delay(10);
  }
  return false;
}

// A healthy connect takes 100-300 ms, so three seconds is already ten times the margin. Waiting
// longer only keeps the radio on: a network that has not answered by then is usually gone, and
// the caller comes back in RETRY_MINUTES anyway. That is also why there is no second attempt —
// the next wake is the retry.
#define CONNECT_TIMEOUT_MS 3000

WifiResult connectWiFi() {
  WiFi.mode(WIFI_STA);
  uint32_t t0 = millis();
  radioOnAt = t0;

  WiFi.begin();                                            // stored by the setup portal
  if (waitConnected(CONNECT_TIMEOUT_MS)) {
    uint32_t ms = millis() - t0;
    int rssi = WiFi.RSSI();
    Serial.printf("Wi-Fi connected in %u ms, RSSI=%d dBm, IP=%s\n",
                  ms, rssi, WiFi.localIP().toString().c_str());
    return { true, ms, rssi };
  }
  Serial.printf("Wi-Fi failed (status %d)\n", WiFi.status());
  return { false, millis() - t0, 0 };   // give up -> caller sleeps & retries next wake
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

uint32_t wifiOff() {
  // true brings the STA interface down so the radio stops. The second argument stays false:
  // erasing the AP config would write to NVS and slow down the next connect.
  WiFi.disconnect(true);
  return millis() - radioOnAt;
}

String httpGet(const char* url) {
  for (int attempt = 1; attempt <= 2; attempt++) {   // 1 try + 1 retry (server cold start)
    WiFiClientSecure client;                         // fresh client each attempt (no stale TLS)
    client.setInsecure();
    HTTPClient http;
    if (http.begin(client, url)) {
      int code = http.GET();
      if (code == 200) {                             // success -> return body
        String out = http.getString();
        http.end();
        return out;
      }
      Serial.printf("httpGet attempt %d failed (code %d)\n", attempt, code);
      http.end();
    }
    if (attempt < 2) delay(500);                     // let a cold server wake before retrying
  }
  return "";   // both attempts failed -> caller sleeps & retries next wake
}

String httpPost(const char* url, const String& body) {
  for (int attempt = 1; attempt <= 2; attempt++) {   // 1 try + 1 retry (server cold start)
    WiFiClientSecure client;                         // fresh client each attempt (no stale TLS)
    client.setInsecure();
    HTTPClient http;
    if (http.begin(client, url)) {
      http.addHeader("Content-Type", "text/plain");
      int code = http.POST(body);
      if (code == 200) {                             // success -> return body
        String out = http.getString();
        http.end();
        return out;
      }
      Serial.printf("httpPost attempt %d failed (code %d)\n", attempt, code);
      http.end();
    }
    if (attempt < 2) delay(500);
  }
  return "";   // both attempts failed -> caller sleeps & retries next wake
}
