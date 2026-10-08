#pragma once
#include <Arduino.h>

// Conditions where the board is sitting, from the SHT40 on I2C. Neither figure is available
// anywhere else: the chip temperature reads the die, which runs warmer than the room, and the
// humidity the server sends is outdoors at the weather location.
struct Room {
  bool  ok = false;   // false if the sensor did not answer — the board works without one
  float c  = 0.0f;
  float rh = 0.0f;

  // Why it failed, sent on every wake. Without this a failed reading arrives as nothing but an
  // absent room_c, and the faults behind that are not the same thing: an open wire, a module
  // with no power, a sensor that never answers its address, and one that answers and then
  // returns corrupt bytes all want different fixes.
  const char* err = "none";  // ok / probe / write / read / crc
  bool   sdaUp = false;      // the module's pull-up still wins against the chip's pull-down
  bool   sclUp = false;
  String ack;                // every address that answered a scan, hex — only after a failure
};

Room readRoom();
