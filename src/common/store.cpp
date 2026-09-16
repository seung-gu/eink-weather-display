#include "store.h"
#include <Preferences.h>
#include "nvs.h"                    // IDF's, for nvs_get_stats — ours is store.h and does not clash
#include "config.h"

static const char* NS = "weather";
static const char* LOG_KEY = "faillog";
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

String lastLog() {
  prefs.begin(NS, true);            // read-only
  String log = prefs.getString(LOG_KEY, "");
  prefs.end();
  return log;
}

void appendLog(const String& entry) {
  prefs.begin(NS, false);
  String log = prefs.getString(LOG_KEY, "");

  // Stop at a marker instead of letting the array outgrow what NVS will take: putString fails
  // silently past its limit, and would take every earlier entry down with it. See LOG_MAX_BYTES.
  String add = entry;
  if (log.length() + entry.length() + 2 > LOG_MAX_BYTES) {
    if (log.endsWith("\"full\"}]")) { prefs.end(); return; }   // already marked, nothing to add
    add = "{\"kind\":\"full\"}";
  }

  // Appended without parsing. The only edit this array ever needs is at the end, and linking a
  // parser in here would add a way for one bad byte to take the whole history with it.
  if (!log.length()) log = "[" + add + "]";
  else { log.remove(log.length() - 1); log += "," + add + "]"; }

  prefs.putString(LOG_KEY, log);
  prefs.end();
}

void clearLog() {
  prefs.begin(NS, false);
  prefs.remove(LOG_KEY);            // not putString(""): NVS rewrites the entry, key and all,
  prefs.end();                      // so an empty write would still burn a slot every wake
}

uint32_t lastAwakeMs() {
  prefs.begin(NS, true);            // read-only
  uint32_t ms = prefs.getUInt("awake", 0);
  prefs.end();
  return ms;
}

void saveAwakeMs(uint32_t ms) {
  prefs.begin(NS, false);
  prefs.putUInt("awake", ms);
  prefs.end();
}

uint16_t nvsFreeEntries() {
  nvs_stats_t st;
  if (nvs_get_stats(NULL, &st) != ESP_OK) return 0;
  return st.free_entries;
}

uint16_t nvsTotalEntries() {
  nvs_stats_t st;
  if (nvs_get_stats(NULL, &st) != ESP_OK) return 0;
  return st.total_entries;
}
