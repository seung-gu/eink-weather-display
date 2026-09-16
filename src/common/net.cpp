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

// http.begin() turning down the URL is a bug in the firmware, not a network that is down. -1
// would say the same thing as HTTPC_ERROR_CONNECTION_REFUSED, which is a good URL and a dead
// server — the two want different reactions, so they get different codes in the log.
#define HTTP_BEGIN_FAILED -100

// WiFi.waitForConnectResult() polls every 100 ms, which leaves the radio on for up to that long
// after the association is already done — and rounds every measurement to 100 ms. Same logic,
// ten times finer. Failure codes still bail early instead of waiting out the timeout.
//
// Returns the status it stopped on, so the caller reports what this actually decided on rather
// than reading WiFi.status() again afterwards and getting whatever it says by then.
static wl_status_t waitConnected(uint32_t timeoutMs) {
  uint32_t start = millis();
  wl_status_t s = WiFi.status();
  while (millis() - start < timeoutMs) {
    s = WiFi.status();
    if (s == WL_CONNECTED || s == WL_CONNECT_FAILED || s == WL_NO_SSID_AVAIL) return s;
    delay(10);
  }
  return s;
}

WifiResult connectWiFi() {
  WiFi.mode(WIFI_STA);
  uint32_t t0 = millis();

  WiFi.begin();                                            // stored by the setup portal
  wl_status_t s = waitConnected(CONNECT_TIMEOUT_MS);
  uint32_t ms = millis() - t0;
  if (s == WL_CONNECTED) {
    int rssi = WiFi.RSSI();
    Serial.printf("Wi-Fi connected in %u ms, RSSI=%d dBm, IP=%s\n",
                  ms, rssi, WiFi.localIP().toString().c_str());
    return { true, ms, rssi, (uint8_t)s };
  }
  Serial.printf("Wi-Fi failed (status %d)\n", s);
  return { false, ms, 0, (uint8_t)s };   // give up -> caller sleeps & retries next wake
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
  if (!http.begin(client, url)) return { HTTP_BEGIN_FAILED, "" };

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
  if (!http.begin(client, url)) return { HTTP_BEGIN_FAILED, "" };

  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  String body = (code == 200) ? http.getString() : String();
  if (code != 200) Serial.printf("httpPost failed (code %d)\n", code);
  http.end();
  return { code, body };
}
