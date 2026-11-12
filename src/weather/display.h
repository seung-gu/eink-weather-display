#pragma once
#include <Arduino.h>
#include "room.h"

// Init the e-Paper (remap SPI + manual reset + start fonts)
void displayBegin();

// Drives every line into the panel low, which also switches the module off, and holds them there
// through deep sleep. displayBegin() releases them.
void displayHoldPins();

// Full-screen message: a centred heading over newline-separated body lines, drawn left
// aligned. Used before Wi-Fi exists, so it takes no weather data.
void displayMessage(const String& title, const String& body);

// Registration screen, for a board that is online but not in anyone's account yet. A QR code that
// opens the dashboard with this code filled in, the code itself in large type for typing by hand,
// and what to do next. Full refresh.
void displayRegister(const String& code);

// Render a weather response string (up to 8 lines). A partial refresh over the last weather
// screen; a full one every 12 hours, and when there is none to start from (after a reset, a
// message or a QR).
// updated = did this wake bring a new response. The stored one carries the time it was fetched,
// so when nothing came back the clock is replaced by "offline". rssi >= 0 means "no signal".
void displayWeather(const String& w, bool updated, int rssi, uint32_t batteryMv,
                    const Room& room);

// How long the last screen took to reach the panel: sending it, the refresh, and putting the panel
// to sleep. What a display call took beyond this was drawing it.
uint32_t displayFrameMs();
