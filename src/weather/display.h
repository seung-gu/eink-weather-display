#pragma once
#include <Arduino.h>

// Init the e-Paper (remap SPI + manual reset + start fonts)
void displayBegin();

// Render a weather response string (up to 8 lines). Full refresh.
// fresh = did this wake actually reach the server; false shows "offline" in place of
// the clock, since the stored response carries a stale time. rssi >= 0 means "no signal".
void displayWeather(const String& w, bool fresh, int rssi, uint32_t batteryMv);
