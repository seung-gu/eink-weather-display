#include "net.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "config.h"

WifiResult connectWiFi() {
  WiFi.mode(WIFI_STA);
  uint32_t t0 = millis();

  for (int attempt = 1; attempt <= 2; attempt++) {           // 1 try + 1 retry
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    if (WiFi.waitForConnectResult(10000) == WL_CONNECTED) {  // wait up to 10s
      uint32_t ms = millis() - t0;
      int rssi = WiFi.RSSI();
      Serial.printf("Wi-Fi connected in %u ms, RSSI=%d dBm, IP=%s\n",
                    ms, rssi, WiFi.localIP().toString().c_str());
      return { true, ms, rssi };
    }
    Serial.printf("Wi-Fi attempt %d failed (status %d)\n", attempt, WiFi.status());
    WiFi.disconnect();
  }
  return { false, millis() - t0, 0 };   // give up -> caller sleeps & retries next wake
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
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
    delay(500);                                      // brief pause -> let a cold server wake
  }
  return "";   // both attempts failed -> caller sleeps & retries next wake
}
