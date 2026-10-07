#pragma once
#include <Arduino.h>

// Conditions where the board is sitting, from the SHT40 on I2C. Neither figure is available
// anywhere else: the chip temperature reads the die, which runs warmer than the room, and the
// humidity the server sends is outdoors at the weather location.
struct Room {
  bool  ok = false;   // false if the sensor did not answer — the board works without one
  float c  = 0.0f;
  float rh = 0.0f;
  bool  stuck = false;  // SDA was being held low when the wake started, so the bus had latched
                        // and was cleared before reading. Reported, because nothing else says
                        // whether a failed read found a jammed bus or a quiet sensor
};

Room readRoom();
