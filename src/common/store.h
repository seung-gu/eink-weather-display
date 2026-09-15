#pragma once
#include <Arduino.h>

// What the board remembers across deep sleep, resets and power loss. One namespace, opened and
// closed inside each call, so nothing else has to know it is NVS underneath.
//
// Wi-Fi credentials are NOT here: the Wi-Fi driver keeps those in its own namespace.

// Record that a connect is about to be attempted and return how many have gone unanswered.
// Counting before the attempt is what makes an interrupted boot count, so repeated resets
// reach the limit the same way repeated failures do.
uint8_t recordWifiAttempt();
void    clearWifiAttempts();

// The last weather body the server sent. Empty until the first successful fetch.
String  lastWeather();
void    saveWeather(const String& body);
