#include "room.h"
#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include "config.h"

static Adafruit_SHT4x sht;

Room readRoom() {
  Wire.begin(SHT_SDA, SHT_SCL);
  // Defaults are high precision with the heater off, which is what this wants. The heater is for
  // clearing condensation and draws tens of mA — on a battery it would cost more than the wake.
  if (!sht.begin(&Wire)) {
    Serial.println("SHT40 not found");
    return {};
  }
  sensors_event_t humidity, temp;
  if (!sht.getEvent(&humidity, &temp)) return {};
  return { true, temp.temperature, humidity.relative_humidity };
}
