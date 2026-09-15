#pragma once
#include <Arduino.h>

// Init the e-Paper (remap SPI + manual reset + start fonts)
void displayBegin();

// Full-screen message: a centred heading over newline-separated body lines, drawn left
// aligned. Used before Wi-Fi exists, so it takes no weather data.
void displayMessage(const String& title, const String& body);

// Render a weather response string (up to 8 lines). Full refresh.
// fresh = did this wake actually reach the server; false shows "offline" in place of
// the clock, since the stored response carries a stale time. rssi >= 0 means "no signal".
void displayWeather(const String& w, bool fresh, int rssi, uint32_t batteryMv);
