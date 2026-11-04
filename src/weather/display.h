#pragma once
#include <Arduino.h>
#include "room.h"

// Init the e-Paper (remap SPI + manual reset + start fonts)
void displayBegin();

// Drives every line into the panel low, which also switches the module off, and holds them there
// through deep sleep. displayBegin() releases them.
void displayHoldPins();

// The same lines low, without holding them: the module off until displayBegin(). Called first
// thing on every boot, because RESET or power-on releases the holds and RST would otherwise float
// and leave the module on, its other lines floating too, through Wi-Fi and the server. One board
// then failed to draw on every RESET wake, while drawing on its timer wakes.
void displayOff();

// Full-screen message: a centred heading over newline-separated body lines, drawn left
// aligned. Used before Wi-Fi exists, so it takes no weather data.
void displayMessage(const String& title, const String& body);

// Registration screen, for a board that is online but not in anyone's account yet. A QR code that
// opens the dashboard with this code filled in, the code itself in large type for typing by hand,
// and what to do next. Full refresh.
void displayRegister(const String& code);

// Render a weather response string (up to 8 lines). Full refresh.
// updated = did this wake bring a new response. The stored one carries the time it was fetched,
// so when nothing came back the clock is replaced by "offline". rssi >= 0 means "no signal".
void displayWeather(const String& w, bool updated, int rssi, uint32_t batteryMv,
                    const Room& room);
