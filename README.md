[English](README.md) | [한국어](README.ko.md)

# eink-weather-display

A low-power weather display built with an **ESP32-C3 + 1.54" e-Paper**.
Fetches pre-formatted weather from a server (MCP) over HTTP, renders it on e-ink, then deep-sleeps — running for months on a battery.

> Topics: `esp32-c3` · `e-ink` · `mcp` · `battery`

## Runtime (5 steps)
![Runtime: wake, Wi-Fi, server GET, draw e-Paper, deep sleep, then reset on wake](docs/runtime.png)

- Everything in `setup()`, `loop()` empty (a wake is a full reset)
- Bistable e-ink → 0 current to hold the image, draws only on refresh
- Persist state across sleeps with `RTC_DATA_ATTR`

## Data pipeline
![Data pipeline: public weather API to MCP server to device; server also exposed over MCP to an AI](docs/pipeline.png)

- Formatting is server-side → the device stays light (less RAM/power)
- Device: one HTTPS GET; AI analyze/control is an optional path

## Hardware
| Part | Used |
|---|---|
| MCU | ESP32-C3 (Seeed XIAO ESP32-C3 or ESP32-C3 Super Mini) |
| Display | Waveshare 1.54" e-Paper (SSD1681, 200×200, black/white) |
| Framework | Arduino (PlatformIO) |
| Power | LiPo battery — months on deep sleep |

## Wiring (`src/config.h`)
Both boards use the **same GPIO numbers** (identical code) — only the physical pin labels differ.

| e-Paper | GPIO | Super Mini pin | XIAO label |
|---|---|---|---|
| VCC | — | 3V3 | 3V3 |
| GND | — | GND | GND |
| DIN (MOSI) | 7 | GPIO7 | D5 |
| CLK (SCK) | 6 | GPIO6 | D4 |
| CS | 10 | GPIO10 | D10 |
| DC | 5 | GPIO5 | D3 |
| RST | 4 | GPIO4 | D2 |
| BUSY | 3 | GPIO3 | D1 |

- Super Mini labels pins by GPIO number; XIAO uses `D0–D10` (board positions mapped to different GPIOs)
- ⚠️ The C3's default SPI pins (SCK=4 / MISO=5) collide with RST(4)/DC(5) → `display.cpp` remaps SPI and re-asserts the pins

## Setup (secrets)
Wi-Fi values live in `secrets.h` (not committed). After cloning:
```bash
cp src/secrets.example.h src/secrets.h   # then fill in WIFI_SSID / WIFI_PASSWORD
```

## Build & Upload
```bash
pio run -t upload
pio device monitor -b 115200
```
- After flashing the deep-sleep firmware, re-uploads need download mode: hold **BOOT**, tap **RESET**

## Layout
```
src/
├─ config.h         all settings (URLs · pins · intervals)
├─ net.h / .cpp     Wi-Fi connect + HTTP GET
├─ display.h / .cpp e-Paper init + rendering (globals kept static = encapsulated)
├─ weather_icons.h  weather icon bitmaps
└─ main.cpp         flow only (setup/loop)
```

## Design notes
- [Persistence & refresh](docs/persistence-and-refresh.md) — why the last weather is kept in NVS
  (not RTC memory), and why a partial refresh of just the status line doesn't work here
- [JTAG debugging](docs/jtag-debug.md) — step debugging over the built-in USB-JTAG: the OpenOCD
  overrides it needs, and why download mode silently breaks it

## Server (MCP)
The weather/LED backend is a companion project: **[seung-gu/emcp](https://github.com/seung-gu/emcp)**.

- The device only hits a plain HTTPS GET endpoint
- The same server is exposed over MCP, so an AI client (e.g. ChatGPT) can query and control it
