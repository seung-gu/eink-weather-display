#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Where one wake's time goes, in the order it is spent. Each step runs from the end of the one
// before it to its own end, so together they cover the wake from the firmware's start to the
// moment before sleep: they add up to the awake time. The next report carries them, the way it
// carries the awake time, and the dashboard shows them side by side to find which one grew.
enum Step : uint8_t {
  STEP_SENSE,       // firmware start to just before Wi-Fi: the readings and the NVS bookkeeping
  STEP_RADIO_ON,    // bringing the radio up
  STEP_JOIN,        // joining the access point and getting an address — the report's wifi_ms
  STEP_DNS,         // looking the server's name up
  STEP_CONNECT,     // TCP and the TLS handshake, the certificate check included
  STEP_WAIT,        // building and sending the report, until the answer starts arriving
  STEP_READ,        // reading the rest of the answer and closing the connection
  STEP_SAVE,        // storing the weather and clearing the log, or filing the failure
  STEP_RADIO_OFF,   // putting the radio down
  STEP_PANEL,       // waking the panel
  STEP_DRAW,        // drawing the screen into memory
  STEP_REFRESH,     // sending it to the panel and waiting out the refresh
  STEP_FINISH,      // what is left after the screen, until sleep
  STEP_OTA,         // looking for a newer firmware in the answer, and downloading it if there is one
  STEP_COUNT
};

// Ends step s now: the time since the last step ended goes to it. A step reached more than once
// adds up.
void stepDone(Step s);

// Moves ms of the time step `from` holds to step `to`. For a call that timed its own parts, such
// as the request: the whole call goes to one step first, and then each part is moved out of it.
void stepSplit(Step from, Step to, uint32_t ms);

// Stores this wake's steps for the next report. Called with false on a wake that never reached the
// point of reporting, such as the setup portal, which leaves nothing for the next report to carry.
void saveSteps(bool reported);

// Puts the previous wake's steps on a report, as "prev_steps": {name: ms}. Steps that took no
// time are left out.
void addPrevSteps(JsonDocument& report);
