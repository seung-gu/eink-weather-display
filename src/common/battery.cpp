#include "battery.h"
#include "config.h"

uint32_t batteryMillivolts() {
  // analogReadMilliVolts applies the per-chip eFuse calibration, so no attenuation
  // or curve fitting here. Average a few samples: the 1M divider is a high-impedance
  // source and single reads wander.
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(BAT_ADC);
  return (sum / 8) * 2;   // undo the 1:1 divider
}
