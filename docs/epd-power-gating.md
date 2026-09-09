[English](epd-power-gating.md) | [한국어](epd-power-gating.ko.md)

# Powering the e-Paper module from a GPIO

`display.hibernate()` puts the SSD1681 controller to sleep. It does nothing about the rest of the
module: the Waveshare carrier board carries a level shifter and its own supply circuitry, and those
keep drawing from 3V3 for the whole deep sleep.

Pulling the module's VCC wire off the 3V3 pin drops the measured consumption sharply. This note
records how to make that switch automatic, and why the change is not in the firmware.

---

## 1. The change

**Wiring**: move the module's VCC from the board's 3V3 pin to **D7 (GPIO20)**.

**`config.h`**

```c
#define EPD_PWR   20    // module VCC, switched (XIAO D7)
```

**`display.cpp` — power the module before talking to it**

```cpp
void displayBegin() {
  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, HIGH);
  delay(10);                  // let the rail settle before the reset pulse
  display.init(115200);
  ...
```

The delay matters: `display.init()` and the manual RST pulse follow immediately, and a reset issued
before the rail has risen leaves the controller half-initialised.

**`display.cpp` — cut power on the way out**

```cpp
void displayPowerOff() {
  for (int p : {EPD_CS, EPD_DC, EPD_RST, EPD_SCK, EPD_MOSI, EPD_PWR}) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
  pinMode(EPD_BUSY, INPUT);
}
```

**`main.cpp` — right before deep sleep**

```cpp
#include "driver/gpio.h"
...
displayPowerOff();
gpio_deep_sleep_hold_en();
esp_deep_sleep_start();
```

---

## 2. Why the pins go low before the power

A GPIO left HIGH against an unpowered module feeds current through the input clamp diodes into the
module's dead supply rail, which keeps part of the board alive. Every line the firmware drives goes
to 0 V first, and `EPD_PWR` goes last.

**BUSY is the exception.** It is the module's output. Driving it as an output puts two drivers on
one net, which shorts them together for as long as both are powered. It stays an input.

---

## 3. Why D7 and not D6

| Pin | |
|---|---|
| D0 (GPIO2), D8 (GPIO8), D9 (GPIO9) | strapping pins — sampled at boot |
| D6 (GPIO21) | UART0 TX. The ROM bootloader drives it at every boot and UART idles high |
| **D7 (GPIO20)** | UART0 RX — an input at boot, never driven by the ROM |

Serial output on this board goes over native USB, so UART0 is free for other use.

---

## 4. `gpio_deep_sleep_hold_en()` has a cost

Without it every pad returns to its default state on the way into sleep, so `EPD_PWR` stops holding
the module off. With it, the pins stay where `displayPowerOff()` left them.

The header states what that costs (`driver/gpio.h`):

> The state of each pad holds is its active configuration (**not pad's sleep configuration!**)

ESP-IDF normally moves pads to a lower-power sleep configuration. Holding them keeps input buffers
and pull resistors as they were in active mode, so any pin left floating draws crowbar current for
the whole sleep. Pins that are not driven should be given a pull rather than left floating —
`INPUT_PULLDOWN` on BUSY, for instance.

---

## 5. Why this is not in the firmware

The benefit was never measured. Consumption was tracked by watching battery voltage, and a lithium
cell taken off the charger keeps falling on its own for hours as surface charge dissipates — the
slope flattens monotonically no matter what the firmware does. Every change measured that way looks
like an improvement, because time passed between the readings.

Once the cell had settled, gated (D7) and ungated (3V3) measured the same.

---

## 6. Measuring it properly

The mechanism is real, so the question is only how much it is worth. Voltage cannot answer it:

| | |
|---|---|
| **Ammeter in series, µA range** | Settles it in one reading. A deep-sleep floor of tens of µA against milliamps is not a subtle difference |
| Voltage over days | Only after the cell has rested, and only comparing equal spans at similar states of charge |

Worth checking against the wake budget before spending effort here. At a 10-minute period, roughly
12 s awake at ~100 mA costs about 48 mAh/day, while an ideal deep sleep floor costs about 1 mAh/day.
The wake cycle dominates unless the sleep floor is in milliamps.
