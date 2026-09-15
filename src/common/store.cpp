#include "store.h"
#include <Preferences.h>

static const char* NS = "weather";
static Preferences prefs;

uint8_t recordWifiAttempt() {
  prefs.begin(NS, false);
  uint8_t n = prefs.getUChar("wififail", 0) + 1;
  prefs.putUChar("wififail", n);
  prefs.end();
  return n;
}

void clearWifiAttempts() {
  prefs.begin(NS, false);
  prefs.putUChar("wififail", 0);
  prefs.end();
}

String lastWeather() {
  prefs.begin(NS, true);            // read-only
  String body = prefs.getString("last", "");
  prefs.end();
  return body;
}

void saveWeather(const String& body) {
  prefs.begin(NS, false);
  prefs.putString("last", body);
  prefs.end();
}
