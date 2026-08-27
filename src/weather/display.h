#pragma once
#include <Arduino.h>

// Init the e-Paper (remap SPI + manual reset + start fonts)
void displayBegin();

// Render a weather response string (up to 7 lines). Full refresh.
// wifiMs = 0 (offline) shows "offline"; rssi >= 0 means "no signal".
void displayWeather(const String& w, uint32_t wifiMs, int rssi);
