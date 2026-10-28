[English](deep-sleep-drain.md) | [한국어](../ko/deep-sleep-drain.md)

# Battery leakage current troubleshooting

From 2026-10-06 the XIAO board (mac E8F60A17AF28) drained at about −9 mV/h instead of about
−1.5 mV/h.

**Cause:** on the Waveshare V2.1 e-Paper board RST also switches the module's supply, and the
firmware never held RST low through deep sleep. It floated until 7110254 and was held high after
that, and either way the module stayed powered. f880792 holds every line into the panel low,
RST included.

---

## 1. On the V2.1 board, RST is also the power switch

From the Waveshare 1.54" e-Paper Module schematic. This board is V2.1.

- RST → R35 1 kΩ → base of Q32 (S8050), with R34 47 kΩ from base to GND. Q32 pulls the gate of Q31
  (AO3401 P-MOSFET, 100 kΩ gate pull-up) low, which connects the module's supply.
- RST → R36 1 kΩ → VCC_EN, with R37 47 kΩ to GND. VCC_EN is the enable of the RT9193-33 LDO.
- A TXS0108E level shifter sits between the header and the panel.

RST high: module powered. RST low: module off. Measured RST–GND on this board: 16.5 kΩ.

Held high, RST drives about (3.3 − 0.7) V / 1 kΩ ≈ 2.6 mA into Q32's base. Computed from the
schematic, not measured.

The same is said elsewhere:
- GxEPD2's `_reset()` (`GxEPD2_EPD.cpp`) is commented "NEEDED for Waveshare clever reset circuit".
- The Waveshare wiki says a long reset low can power-cycle the driver board.

[Powering the e-Paper module from a GPIO](epd-power-gating.md) wires a GPIO into the module's VCC
to switch it off. On the V2.1 board RST already does that.

---

## 2. What the firmware did with RST

| Firmware | RST in deep sleep | Module in deep sleep |
|---|---|---|
| before 7110254 | not held, floating | powered: BUSY read 3.3 V. Touching RST with a meter dropped BUSY to 0 V |
| 7110254, 0e14a8d | held high | powered |
| f880792 | held low | off |

---

## 3. The fix

`displayHoldPins()` (`display.cpp`), called from `deepSleep()` (`main.cpp`) on both ways into
sleep:
- `SPI.end()`, so CLK and DIN are plain pins again.
- RST, CS, DC, CLK and DIN driven low and held with `gpio_hold_en()`.
- BUSY is the module's output and is left alone.

Every line goes low, not only RST: a line left high would feed the unpowered module through the
level shifter's clamp diodes.

`displayBegin()` releases the holds before anything else.

To check a sleeping board: D2 (RST) and D7 (BUSY) read 0 V.

---

## 4. Pins in deep sleep on the C3

- GPIO0–5 are never held unless `gpio_hold_en()` is called on each. RST is GPIO4, DC is GPIO5.
- With `gpio_deep_sleep_hold_en()`, IDF turns off input, output and pulls on every GPIO6–21 that is
  not held by itself (`esp_sleep_isolate_digital_gpio()`, `sleep_gpio.c`).
- A pad held by itself stays held in deep sleep only while `gpio_deep_sleep_hold_en()` is on.
- A held pad ignores writes until `gpio_hold_dis()`.

Measured with the board asleep before f880792: DC, CLK, DIN and CS each read 0 V on one probe and
3.3 V on the next, with no wake in between.

---

## 5. Ruled out on the way

- **Code:** the old d03bffd build drained the same.
- **SHT40 sensor:** removed entirely, no change.
- **Battery:** a new cell drained the same.
- **Battery divider**
- **A faulty module:** swapping in another one on 10-16 gave about 13 h flat, then the drain came
  back on 10-18 with that same module.

---

## 6. Slopes on 7110254

| Local time (UTC+2) | Module | Slope |
|---|---|---|
| 10-16 16:00–20:00 | A | −9.5 mV/h |
| 10-16 21:00 – 10-17 10:33 | X (fitted 20:20) | −1.2 mV/h |
| 10-17 11:39 – 10-18 09:57 | Y (fitted 10:46–11:09) | −4 mV/h |
| 10-18 11:26 – 21:27 | X again (fitted 10:01–10:56) | −10 mV/h |

0e14a8d drained as fast.

---

## 7. Still unexplained

- The −1.2 mV/h stretch on 7110254. RST was held high, so the module was powered for the whole
  sleep.
- Why the drain began on 2026-10-06.
- Where the current that kept the module on came from while RST floated.
