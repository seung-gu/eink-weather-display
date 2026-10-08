#include "room.h"
#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include "config.h"

// Does the module's pull-up still win against the chip's own pull-down? The breakout's resistor
// is around 10k and the C3's internal one around 45k, so a connected idle line divides to 2.7 V
// and reads high, while an open wire or an unpowered module leaves the pull-down alone and reads
// low. That separates a broken connection from a sensor that is wired up and silent.
//
// Self-checking: on a wake that read the sensor fine, both of these have to come back true. If
// they do not, the breakout's pull-ups are weaker than 45k and the test means nothing here.
static bool pullUpPresent(int pin) {
  pinMode(pin, INPUT_PULLDOWN);
  delayMicroseconds(50);          // the line has to climb through its own capacitance first
  bool up = digitalRead(pin) == HIGH;
  pinMode(pin, INPUT);
  return up;
}

// Every address that answers, as hex. Run only after a failed reading: it is a transaction per
// address, and on a good wake it says nothing the reading has not already said. Nothing at all
// answering means the bus is dead rather than the sensor.
static String scanBus() {
  String found;
  for (uint8_t a = 8; a < 120; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      if (found.length()) found += "-";
      found += String(a, HEX);
    }
  }
  return found;
}

// Read it here rather than through Adafruit_SHT4x, which collapses four different failures into
// one false: the address not answering, the command write being refused, the six bytes not
// coming back, and the CRC not matching. The last one is the interesting one — it means the bus
// works and the bytes are arriving damaged, which is a signal problem rather than a dead sensor.
// Everything below follows the same sequence the library uses, including the 10 ms the datasheet
// gives a high-precision measurement.
static uint8_t crc8(const uint8_t* data, int len) {
  uint8_t crc = 0xFF;
  for (int i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++)
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

Room readRoom() {
  Room r;
  // Before Wire.begin(), while the pins are still ours to poke at.
  r.sdaUp = pullUpPresent(SHT_SDA);
  r.sclUp = pullUpPresent(SHT_SCL);
  Wire.begin(SHT_SDA, SHT_SCL);

  Wire.beginTransmission(SHT4x_DEFAULT_ADDR);
  if (Wire.endTransmission() != 0) { r.err = "probe"; r.ack = scanBus(); return r; }

  // High precision with the heater off. The heater is for clearing condensation and draws tens
  // of mA — on a battery it would cost more than the whole wake.
  Wire.beginTransmission(SHT4x_DEFAULT_ADDR);
  Wire.write((uint8_t)SHT4x_NOHEAT_HIGHPRECISION);
  if (Wire.endTransmission() != 0) { r.err = "write"; r.ack = scanBus(); return r; }
  delay(10);

  uint8_t b[6];
  if (Wire.requestFrom((uint8_t)SHT4x_DEFAULT_ADDR, (uint8_t)6) != 6) {
    r.err = "read"; r.ack = scanBus(); return r;
  }
  for (uint8_t& v : b) v = Wire.read();
  if (b[2] != crc8(b, 2) || b[5] != crc8(b + 3, 2)) {
    r.err = "crc"; r.ack = scanBus(); return r;
  }

  r.ok = true;
  r.err = "ok";
  r.c  = -45.0f + 175.0f * ((b[0] << 8) | b[1]) / 65535.0f;
  r.rh = -6.0f + 125.0f * ((b[3] << 8) | b[4]) / 65535.0f;
  r.rh = constrain(r.rh, 0.0f, 100.0f);
  return r;
}
