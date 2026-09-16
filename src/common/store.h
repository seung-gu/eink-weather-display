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

// Wakes the server never heard about, as a JSON array, oldest first. Empty while there is
// nothing to report, so "no key" and "empty log" are one state and emptiness needs no encoding.
// appendLog() takes one finished JSON object, braces included: what the fields mean is the
// caller's business, the same way this file knows nothing about the weather text it stores.
// clearLog() runs only after a 200 — a report that did not land has to be sent again.
String  lastLog();
void    appendLog(const String& entry);
void    clearLog();
