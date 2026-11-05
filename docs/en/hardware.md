[English](hardware.md) | [한국어](../ko/hardware.md)

# Hardware and wiring

| Part | Used |
|---|---|
| MCU | Seeed XIAO ESP32-C3 |
| Display | Waveshare 1.54" e-Paper V2.1 (SSD1681, 200×200, black/white) |
| Room sensor | SHT40 breakout on I2C, optional |
| Power | LiPo battery |

Every pin is set in `src/common/config.h`. XIAO labels its pins `D0–D10` by their place on the board, and those labels do not match the GPIO numbers.

## e-Paper
| e-Paper | GPIO | XIAO label |
|---|---|---|
| VCC | — | 3V3 |
| GND | — | GND |
| DIN (MOSI) | 7 | D5 |
| CLK (SCK) | 6 | D4 |
| CS | 10 | D10 |
| DC | 5 | D3 |
| RST | 4 | D2 |
| BUSY | 20 | D7 |

- On the V2.1 board RST also switches the module's supply, so the firmware holds it and the other panel lines low through deep sleep. See [Battery leakage current troubleshooting](deep-sleep-drain.md)
- The C3's default SPI pins (SCK=4, MISO=5) collide with RST (4) and DC (5), so `display.cpp` calls `SPI.begin(6, -1, 7, 10)`, sets the panel pins as outputs again and resets the panel by hand
- RST and BUSY both stay wired. GxEPD2 accepts `-1` for either, falling back to a software reset and to fixed delays; tried here, and the display did not render correctly. Cause not investigated

## Battery voltage
Shown on the display and reported to the server.

| | GPIO | XIAO label |
|---|---|---|
| Divider tap | 3 | D1 |

`BAT+ ──[1M]──┬──[1M]── GND`, tap to D1, plus `100nF` from tap to GND. ADC2 stops working once Wi-Fi is on, so the tap has to sit on ADC1 (GPIO0–4). GPIO2 is a strapping pin that reads low through the divider at boot, which is why BUSY moved off GPIO3 to make room.

## Room temperature and humidity (SHT40)
The board works without one.

| SHT40 | GPIO | XIAO label |
|---|---|---|
| VIN | — | 3V3 |
| GND | — | GND |
| SDA | 8 | D8 |
| SCL | 2 | D0 |

The pins XIAO prints as SDA and SCL are taken by the e-Paper's SPI, so the bus moves. GPIO21 would be the obvious free pin, but it is UART0 TX, which the ROM bootloader drives at every reset. GPIO9 is free too, and worse: it picks the boot mode, so a bus left stuck with SDA low would boot to download mode and never wake again.
