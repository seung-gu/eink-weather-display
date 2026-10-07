#include "room.h"
#include <Wire.h>
#include <Adafruit_SHT4x.h>
#include "config.h"

static Adafruit_SHT4x sht;

// Both lines are open-drain, so only the pull-up raises them: SDA found low means the far end is
// holding it down. That is a slave left mid-byte, waiting for a clock that stopped coming — the
// Wire transaction timed out at 50 ms and gave up where it was instead of finishing the byte. It
// stays there across deep sleep, since the sensor keeps its supply, and nothing can start over a
// low SDA because START needs a falling edge on it. Clocking out the rest of the byte releases
// it: 8 bits plus the ACK slot is 9, which covers wherever it stopped. Then a STOP leaves the
// bus idle. The sensor's own soft reset cannot do this — that command is itself an I2C write.
static bool unstickBus() {
  pinMode(SHT_SDA, INPUT);
  if (digitalRead(SHT_SDA) == HIGH) return false;

  pinMode(SHT_SCL, OUTPUT);
  for (int i = 0; i < 9; i++) {
    digitalWrite(SHT_SCL, LOW);  delayMicroseconds(5);
    digitalWrite(SHT_SCL, HIGH); delayMicroseconds(5);
  }
  digitalWrite(SHT_SDA, LOW);    // latch first, then drive: going OUTPUT on a stale HIGH would
  pinMode(SHT_SDA, OUTPUT);      // push against a slave that is still pulling the line down
  delayMicroseconds(5);
  pinMode(SHT_SDA, INPUT);       // released while SCL is high — that rising edge is the STOP
  delayMicroseconds(5);
  return true;
}

Room readRoom() {
  Room r;
  r.stuck = unstickBus();
  Wire.begin(SHT_SDA, SHT_SCL);
  // Defaults are high precision with the heater off, which is what this wants. The heater is for
  // clearing condensation and draws tens of mA — on a battery it would cost more than the wake.
  if (!sht.begin(&Wire)) {
    Serial.println("SHT40 not found");
    return r;
  }
  sensors_event_t humidity, temp;
  if (!sht.getEvent(&humidity, &temp)) return r;
  return { true, temp.temperature, humidity.relative_humidity, r.stuck };
}
