#pragma once
#include <Arduino.h>

// Init the e-Paper (remap SPI + manual reset + start fonts)
void displayBegin();

// Full-screen message: a centred heading over newline-separated body lines, drawn left
// aligned. Used before Wi-Fi exists, so it takes no weather data.
void displayMessage(const String& title, const String& body);

// Render a weather response string (up to 8 lines). Full refresh.
// updated = did this wake bring a new response. The stored one carries the time it was fetched,
// so when nothing came back the clock is replaced by "offline". rssi >= 0 means "no signal".
void displayWeather(const String& w, bool updated, int rssi, uint32_t batteryMv);
