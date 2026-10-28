[English](deep-sleep-drain.md) | [한국어](../ko/deep-sleep-drain.md)

# Battery leakage current troubleshooting

From 2026-10-06 the XIAO board (mac E8F60A17AF28) drained at about −9 mV/h instead of about
−1.5 mV/h.

**Cause:** on the Waveshare V2.1 e-Paper board RST also switches the module's supply, and the
firmware never held RST low through deep sleep. It floated until 7110254 and was held high after
that, and either way the module stayed powered. f880792 holds every line into the panel low,
RST included.

**Result:** with f880792 on the board from 2026-10-21 15:52, the battery went from 3970 to 3862 mV
in 6.3 days: −0.76 mV/h. The two builds before it drained at −3.6 and −7.6 mV/h (section 6). It
was flashed from the working tree before the commit, so the board reports it as `0e14a8d-dirty`.

---

## 1. On the V2.1 board, RST also switches the module

On the V2.1 board RST has to be low for the module to switch off and stop drawing current. Held
high, the module stays on through deep sleep.

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

## 6. Slopes

Each span starts at least an hour after a charge. Local time, Europe/Berlin.

| Firmware | Span | Battery | Slope |
|---|---|---|---|
| 7110254 | 10-19 11:15 – 10-20 14:27 | 3966 → 3866 mV | −3.6 mV/h |
| 0e14a8d | 10-20 15:40 – 10-21 15:28 | 4172 → 3990 mV | −7.6 mV/h |
| f880792 | 10-21 17:35 – 10-28 01:53 | 3970 → 3862 mV | −0.76 mV/h |

Earlier on 7110254, while the module was being swapped:

| Local time (UTC+2) | Module | Slope |
|---|---|---|
| 10-16 16:00–20:00 | A | −9.5 mV/h |
| 10-16 21:00 – 10-17 10:33 | X (fitted 20:20) | −1.2 mV/h |
| 10-17 11:39 – 10-18 09:57 | Y (fitted 10:46–11:09) | −4 mV/h |
| 10-18 11:26 – 21:27 | X again (fitted 10:01–10:56) | −10 mV/h |

---

## 7. Still unexplained

- The −1.2 mV/h stretch on 7110254. RST was held high, so the module was powered for the whole
  sleep.
- Why the drain began on 2026-10-06.
- Where the current that kept the module on came from while RST floated.
