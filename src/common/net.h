#pragma once
#include <Arduino.h>

// Result of a Wi-Fi connect attempt
struct WifiResult {
  bool ok;        // connected?
  uint32_t ms;    // connect time in ms
  int rssi;       // signal strength in dBm (valid when ok)
};

// Connect to Wi-Fi (retries with timeout). Returns {ok, ms} — caller handles failure.
WifiResult connectWiFi();

// Current connection state
bool wifiConnected();

// HTTPS GET -> response body string (empty on failure)
String httpGet(const char* url);

// HTTPS POST of a plain-text body. Same contract as httpGet: response body, empty on failure.
String httpPost(const char* url, const String& body);

// Drop the connection and power the radio down. Call it once the last request is done —
// everything after that (NVS, e-Paper) runs for seconds with no need for Wi-Fi.
// Returns how long the radio was on, counted from connectWiFi(). That whole window draws
// ~100 mA, so it is the figure the battery actually pays; the connect time is only part of it.
uint32_t wifiOff();
