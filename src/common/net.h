#pragma once
#include <Arduino.h>

// Result of a Wi-Fi connect attempt
struct WifiResult {
  bool ok;         // connected?
  uint32_t ms;     // connect time in ms
  int rssi;        // signal strength in dBm (valid when ok)
  uint8_t status;  // the wl_status_t it stopped on. 1 and 4 mean the network was not there or
                   // the password was wrong; 0 and 6 mean it was there and the join still failed
};

// Connect to Wi-Fi (one attempt, CONNECT_TIMEOUT_MS). Returns {ok, ms} — caller handles failure.
WifiResult connectWiFi();

// Drop the connection and power the radio down. Call it once the last request is done —
// everything after that (NVS, e-Paper) runs for seconds with no need for Wi-Fi.
void disconnectWifi();

// Current connection state
bool wifiConnected();

// Result of one HTTPS request. Keeping the status separate from the body means an empty body
// is not mistaken for a failure, and lets the caller act on the reason — a 401 once the server
// checks tokens means "not registered yet", which wants a different screen than "fetch failed".
struct HttpResult {
  int code;      // HTTP status, or negative when the request never reached a server
  String body;   // response body, empty unless code is 200
  // How long httpPost() spent on its parts, both tries together when it retried. httpGet() leaves
  // them at zero. What the call took beyond them is sending the request and waiting for the answer.
  uint32_t dnsMs = 0;       // looking the server's name up
  uint32_t connectMs = 0;   // TCP and the TLS handshake
  uint32_t readMs = 0;      // reading the body and closing
};

// Every HTTPS request checks the server's certificate against the roots in roots.h. The board
// has no clock, so only the chain is checked, not the dates.
class WiFiClientSecure;
void trustServer(WiFiClientSecure& client);

// httpGet() does not retry: the caller comes back after RETRY_MINUTES anyway. httpPost() retries
// once, and only when the connect never completed: a POST has a side effect on the server, and a
// report that arrived must not be filed twice.
HttpResult httpGet(const char* url);
HttpResult httpPost(const char* url, const String& body);

// The radio's MAC, as "A1B2C3D4E5F6". Burned into the chip, so it is the same string across
// reflashes and NVS erases — which is what makes it usable as a name for the board.
String readMacAddress();