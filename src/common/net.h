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
