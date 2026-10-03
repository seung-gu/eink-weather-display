#pragma once
#include <Arduino.h>

// Conditions where the board is sitting, from the SHT40 on I2C. Neither figure is available
// anywhere else: the chip temperature reads the die, which runs warmer than the room, and the
// humidity the server sends is outdoors at the weather location.
struct Room {
  bool  ok = false;   // false if the sensor did not answer — the board works without one
  float c  = 0.0f;
  float rh = 0.0f;
};

Room readRoom();
