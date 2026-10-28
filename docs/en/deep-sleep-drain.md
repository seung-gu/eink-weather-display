[English](deep-sleep-drain.md) | [한국어](../ko/deep-sleep-drain.md)

# Battery leakage current troubleshooting

Board: XIAO ESP32-C3 (mac E8F60A17AF28), e-Paper: Waveshare 1.54" V2.1

---

## 1. Symptom

From 2026-10-06 the battery ran down far faster than usual. The voltage that normally fell about
1.5 mV an hour fell about 9 mV an hour.

---

## 2. What was tried

| Tried | Result |
|---|---|
| Went back to old firmware (d03bffd) | Just as fast |
| Removed the SHT40 sensor | No change |
| Replaced the battery | Just as fast |
| Checked the battery voltage divider | Fine |
| Held RST high during sleep (7110254) | Mostly still fast (−3.6 to −10 mV/h) |
| Swapped the e-Paper module (10-16, 10-18) | Normal for about 13 h, then fast again |
| Found the panel lines flipping between 0 V and 3.3 V during sleep, held them all. RST high (0e14a8d) | Still fast (−7.6 mV/h) |

---

## 3. Fix

Every line into the panel (RST, CS, DC, CLK, DIN) is held low during sleep (f880792). The code is
`displayHoldPins()` in `display.cpp`, called from `deepSleep()` just before sleeping.

Flashed on 2026-10-21 15:52, it has fallen 0.76 mV an hour over 6 days, lower even than the
1.5 mV/h before the problem.

| Firmware | Span (German time) | Battery | Per hour |
|---|---|---|---|
| 7110254 | 10-19 11:15 – 10-20 14:27 | 3966 → 3866 mV | −3.6 mV |
| 0e14a8d | 10-20 15:40 – 10-21 15:28 | 4172 → 3990 mV | −7.6 mV |
| f880792 | 10-21 17:35 – 10-28 01:53 | 3970 → 3862 mV | −0.76 mV |

f880792 was flashed before it was committed, so the dashboard shows it as `0e14a8d-dirty`.

---

## 4. Root cause

On the V2.1 board the module only switches off with RST low. High or floating, the module stays on
through sleep and keeps drawing current.

The firmware never held RST low during sleep.

| Firmware | RST during sleep | Module |
|---|---|---|
| before 7110254 | not held (floating) | on (BUSY 3.3 V) |
| 7110254, 0e14a8d | high | on |
| f880792 | low | off |

The other lines go low too because a line left high leaks current into the switched-off module.

**Not yet explained**
- On 7110254, with RST high, 10-16 21:00 – 10-17 10:33 still ran at a normal −1.2 mV/h.
- Why it started on 10-06.
