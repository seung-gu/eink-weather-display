#include "net.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "config.h"

void connectWiFi() {
  Serial.printf("Wi-Fi connecting: %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.printf(" connected, IP=%s\n", WiFi.localIP().toString().c_str());
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String httpGet(const char* url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String out = "";
  if (http.begin(client, url)) {
    if (http.GET() == 200) out = http.getString();
    http.end();
  }
  return out;
}
