#pragma once
#include <Arduino.h>

// Connect to Wi-Fi (blocks until connected). Returns connect time in ms.
uint32_t connectWiFi();

// Current connection state
bool wifiConnected();

// HTTPS GET -> response body string (empty on failure)
String httpGet(const char* url);
