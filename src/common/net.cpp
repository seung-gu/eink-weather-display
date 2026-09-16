#include "net.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "config.h"

// A healthy connect takes 100-300 ms, so three seconds is already ten times the margin. Waiting
// longer only keeps the radio on: a network that has not answered by then is usually gone, and
// the caller comes back in RETRY_MINUTES anyway. That is also why there is no second attempt —
// the next wake is the retry.
#define CONNECT_TIMEOUT_MS 3000

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

WifiResult connectWiFi() {
  WiFi.mode(WIFI_STA);
  uint32_t t0 = millis();

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

void wifiOff() {
  // true brings the STA interface down so the radio stops. The second argument stays false:
  // erasing the AP config would write to NVS and slow down the next connect.
  WiFi.disconnect(true);
}

HttpResult httpGet(const char* url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) return { -1, "" };

  int code = http.GET();
  String body = (code == 200) ? http.getString() : String();
  if (code != 200) Serial.printf("httpGet failed (code %d)\n", code);
  http.end();
  return { code, body };
}

HttpResult httpPost(const char* url, const String& payload) {
  // No retry: a non-200 means the request arrived and was processed, so sending it again would
  // file the report twice. A failure here is handled like a Wi-Fi failure — by waking sooner.
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, url)) return { -1, "" };

  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  String body = (code == 200) ? http.getString() : String();
  if (code != 200) Serial.printf("httpPost failed (code %d)\n", code);
  http.end();
  return { code, body };
}
