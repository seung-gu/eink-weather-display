#include "steps.h"
#include "store.h"

// The names on the report, in Step order. The server keeps the same list (emcp db/reports.py
// STEP_KEYS) and drops a name it does not know.
static const char* const NAMES[STEP_COUNT] = {
  "sense", "radio_on", "join", "dns", "connect", "wait", "read", "save", "radio_off",
  "panel", "draw", "refresh", "finish", "ota",
};

static uint32_t steps[STEP_COUNT];   // zero on every wake: deep sleep restarts the firmware
static uint32_t lastEnd = 0;         // millis() starts with the firmware, and so does the first step

void stepDone(Step s) {
  uint32_t now = millis();
  steps[s] += now - lastEnd;
  lastEnd = now;
}

void stepSplit(Step from, Step to, uint32_t ms) {
  if (ms > steps[from]) ms = steps[from];   // a part timed a millisecond past the whole
  steps[from] -= ms;
  steps[to]   += ms;
}

void saveSteps(bool reported) {
  saveStepTimes(reported ? steps : nullptr, STEP_COUNT);
}

void addPrevSteps(JsonDocument& report) {
  uint32_t prev[STEP_COUNT];
  if (!lastStepTimes(prev, STEP_COUNT)) return;
  JsonObject o = report["prev_steps"].to<JsonObject>();
  for (int i = 0; i < STEP_COUNT; i++) {
    if (prev[i]) o[NAMES[i]] = prev[i];
  }
}
