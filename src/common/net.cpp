#include "net.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "mbedtls/ssl.h"
#include "config.h"
#include "roots.h"

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

void disconnectWifi() {
  // The first argument brings the STA interface down, which is what actually stops the radio.
  // The second one erases the stored credentials and is left alone: without them the next wake
  // finds nothing to connect to and opens the setup portal, which ends in a sleep with no wake
  // timer. The board would sit there until someone pressed RESET and set up Wi-Fi again.
  WiFi.disconnect(true);
}

void trustServer(WiFiClientSecure& client) {
  client.setCACert(ROOT_CAS);
}

// The server holds an ECDSA and an RSA certificate, and sends the ECDSA one whenever the board
// offers ECDSA at all, which mbedTLS does by default. That chain means checking two P-384
// signatures in software, as the C3 has no hardware for them: the median wake went from 4.46 s to
// 5.72 s when the board started checking. The RSA chain (YR2 <- Root YR, already in roots.h) is
// checked on the RSA accelerator instead. Reports offer only these first. The retry and the
// firmware download keep mbedTLS's own list, so a server that drops its RSA certificate is still
// reached, only as slowly as before.
static const int RSA_SUITES[] = {
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
  MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
  0
};

// What the next connect offers. nullptr leaves mbedTLS's own list.
static const int* nextSuites = nullptr;

// WiFiClientSecure in core 2.0.17 has no way to choose cipher suites: it builds its mbedTLS config
// inside connect(). So the build sends every call to mbedtls_ssl_setup() here instead (-Wl,--wrap
// in platformio.ini). ssl_client.cpp makes that call once the config is complete, just before the
// handshake.
extern "C" int __real_mbedtls_ssl_setup(mbedtls_ssl_context* ssl, const mbedtls_ssl_config* conf);
extern "C" int __wrap_mbedtls_ssl_setup(mbedtls_ssl_context* ssl, const mbedtls_ssl_config* conf) {
  // The config is ssl_client.cpp's own and writable; only this function's signature calls it const.
  if (nextSuites) mbedtls_ssl_conf_ciphersuites(const_cast<mbedtls_ssl_config*>(conf), nextSuites);
  return __real_mbedtls_ssl_setup(ssl, conf);
}

HttpResult httpGet(const char* url) {
  WiFiClientSecure client;
  trustServer(client);
  HTTPClient http;
  if (!http.begin(client, url)) return { HTTP_BEGIN_FAILED, "" };

  int code = http.GET();
  String body = (code == 200) ? http.getString() : String();
  if (code != 200) Serial.printf("httpGet failed (code %d)\n", code);
  http.end();
  return { code, body };
}

// Connects before handing the client to HTTPClient, which takes a client that is already connected
// as it is. POST() would otherwise do the lookup, the handshake and the request in one call, and
// timed apart they show which of them a slow report spent its time on. Every POST goes to
// SERVER_HOST, so that is the host it connects to; a url on another host would need its own here.
static HttpResult post(const char* url, const String& payload, const int* suites) {
  HttpResult r = { 0, "" };
  WiFiClientSecure client;
  trustServer(client);

  uint32_t t = millis();
  IPAddress ip;
  WiFi.hostByName(SERVER_HOST, ip);   // a failure shows up in the connect, which looks it up again
  r.dnsMs = millis() - t;

  t = millis();
  nextSuites = suites;
  // The same connect and timeouts HTTPClient's own connect() uses, so only the timing changes.
  bool up = client.connect(SERVER_HOST, 443, HTTPCLIENT_DEFAULT_TCP_TIMEOUT);
  nextSuites = nullptr;
  r.connectMs = millis() - t;
  if (!up) {
    r.code = HTTPC_ERROR_CONNECTION_REFUSED;   // what POST() returns when its connect fails
    Serial.printf("httpPost failed (code %d)\n", r.code);
    return r;
  }
  client.setTimeout((HTTPCLIENT_DEFAULT_TCP_TIMEOUT + 500) / 1000);

  HTTPClient http;
  if (!http.begin(client, url)) {
    r.code = HTTP_BEGIN_FAILED;
    return r;
  }
  http.addHeader("Content-Type", "application/json");
  r.code = http.POST(payload);
  t = millis();
  if (r.code == 200) r.body = http.getString();
  else Serial.printf("httpPost failed (code %d)\n", r.code);
  http.end();
  r.readMs = millis() - t;
  return r;
}

HttpResult httpPost(const char* url, const String& payload) {
  HttpResult r = post(url, payload, RSA_SUITES);
  // One retry, and only on this code. CONNECTION_REFUSED means the TCP/TLS connect never
  // completed, so nothing was delivered and sending it again cannot file the report twice —
  // which is what rules out retrying the rest. A 500 or a READ_TIMEOUT arrived and was
  // processed; repeating those would duplicate the row on the server. The retry offers
  // mbedTLS's whole list, in case it was the RSA-only offer the server turned down.
  if (r.code == HTTPC_ERROR_CONNECTION_REFUSED) {
    HttpResult again = post(url, payload, nullptr);
    again.dnsMs     += r.dnsMs;       // the first try's time was spent all the same
    again.connectMs += r.connectMs;
    again.readMs    += r.readMs;
    r = again;
  }
  return r;
}

String readMacAddress() {
  String mac = WiFi.macAddress();   // "A1:B2:C3:D4:E5:F6"
  mac.replace(":", "");
  return mac;
}