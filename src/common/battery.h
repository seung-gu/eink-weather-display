#pragma once
#include <Arduino.h>

// Battery voltage in mV, read through the 1:1 divider on BAT_ADC.
uint32_t batteryMillivolts();
