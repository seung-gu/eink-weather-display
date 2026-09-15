#pragma once
#include <Arduino.h>

// Result of a Wi-Fi connect attempt
struct WifiResult {
  bool ok;        // connected?
  uint32_t ms;    // connect time in ms
  int rssi;       // signal strength in dBm (valid when ok)
};

// Connect to Wi-Fi (one attempt, CONNECT_TIMEOUT_MS). Returns {ok, ms} — caller handles failure.
WifiResult connectWiFi();

// Current connection state
bool wifiConnected();

// Result of one HTTPS request. Keeping the status separate from the body means an empty body
// is not mistaken for a failure, and lets the caller act on the reason — a 401 once the server
// checks tokens means "not registered yet", which wants a different screen than "fetch failed".
struct HttpResult {
  int code;      // HTTP status, or negative when the request never reached a server
  String body;   // response body, empty unless code is 200
};

// Neither retries: a POST has a side effect on the server, and the caller comes back after
// RETRY_MINUTES anyway.
HttpResult httpGet(const char* url);
HttpResult httpPost(const char* url, const String& body);

// Drop the connection and power the radio down. Call it once the last request is done —
// everything after that (NVS, e-Paper) runs for seconds with no need for Wi-Fi.
void wifiOff();
