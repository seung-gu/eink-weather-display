#include "net.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "config.h"

uint32_t connectWiFi() {
  uint32_t t0 = millis();                                 // start
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(10);        // poll finely
  uint32_t ms = millis() - t0;                            // elapsed
  Serial.printf("Wi-Fi connected in %u ms, IP=%s\n",
                ms, WiFi.localIP().toString().c_str());
  return ms;
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String httpGet(const char* url) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String out = "";
  if (http.begin(client, url)) {  // ① 어느 가게(URL)에 연결
    if (http.GET() == 200) out = http.getString();  // ② GET 주문 → 200(정상)이면 나온 음식(응답 본문) 받기
    http.end();  // ③ 연결 닫기
  }
  return out;
}
