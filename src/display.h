#pragma once
#include <Arduino.h>

// Init the e-Paper (remap SPI + manual reset + start fonts)
void displayBegin();

// Render a weather response string (up to 7 lines). Call only when it changed.
void displayWeather(const String& w);
