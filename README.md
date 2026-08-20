# eink-weather-display

A low-power weather display built with an **ESP32-C3 + 1.54" e-Paper**.
It fetches pre-formatted weather from a server (MCP) over HTTP, renders it on e-ink, and deep-sleeps to stretch battery life.

> Topics: `esp32-c3` · `e-ink` · `mcp` · `battery`

## Hardware
| Part | Used |
|---|---|
| MCU | ESP32-C3 (Seeed XIAO ESP32-C3 or ESP32-C3 Super Mini) |
| Display | Waveshare 1.54" e-Paper (SSD1681, 200×200, black/white) |
| Framework | Arduino (PlatformIO) |
| Power | LiPo battery — months on deep sleep |

## Wiring (ESP32-C3 Super Mini · see `src/config.h`)
| e-Paper | ESP32-C3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| DIN (MOSI) | GPIO7 |
| CLK (SCK) | GPIO6 |
| CS | GPIO10 |
| DC | GPIO5 |
| RST | GPIO4 |
| BUSY | GPIO3 |

> On the XIAO ESP32-C3 the pin labels (D0–D10) differ — only the physical position changes; adjust the GPIO numbers in `config.h`.
> ⚠️ The C3's default SPI pins (SCK=4 / MISO=5) collide with RST(4)/DC(5), so `display.cpp` re-maps SPI and re-asserts the pins after init.

## Setup (secrets)
Wi-Fi credentials live in `secrets.h`, which is **not** committed. After cloning:
```bash
cp src/secrets.example.h src/secrets.h   # then fill in WIFI_SSID / WIFI_PASSWORD
```

## Build & Upload
```bash
pio run -t upload
pio device monitor -b 115200
```
> After flashing the deep-sleep firmware the board sleeps, so re-uploads need download mode: hold **BOOT**, tap **RESET**, release BOOT.

## Layout
```
src/
├─ config.h         settings (server URLs · pins · intervals)
├─ net.h / .cpp     Wi-Fi connect + HTTP GET
├─ display.h / .cpp e-Paper init + weather rendering
├─ weather_icons.h  weather icon bitmaps
└─ main.cpp         setup/loop (deep-sleep flow)
```

## How it works (5 steps)
```
wake → Wi-Fi → fetch (HTTP GET) → draw on e-Paper → deep sleep → repeat
```
- e-ink holds the image with no power → ~0 draw while asleep
- A deep-sleep wake is a full reset → execution restarts from `setup()`
- Weather is formatted server-side (MCP) so the device stays light

## Server (MCP)
- The device only hits a plain HTTPS GET endpoint
- The same server is also exposed over MCP, so an AI client (e.g. ChatGPT) can query and control it
