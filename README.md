[English](README.md) | [한국어](README.ko.md)

# eink-weather-display

A low-power weather display built with an **ESP32-C3 + 1.54" e-Paper**.
Fetches pre-formatted weather from a server (MCP) over HTTP, renders it on e-ink, then deep-sleeps — running for months on a battery.

> Topics: `esp32-c3` · `e-ink` · `mcp` · `battery`

## Runtime (5 steps)
![Runtime: wake, Wi-Fi, server GET, draw e-Paper, deep sleep, then reset on wake](docs/runtime.png)

- Everything in `setup()`, `loop()` empty (a wake is a full reset)
- Bistable e-ink → 0 current to hold the image, draws only on refresh
- State that has to survive a sleep goes to NVS, not RTC memory (see the design notes)

## Data pipeline
![Data pipeline: public weather API to MCP server to device; server also exposed over MCP to an AI](docs/pipeline.png)

- Formatting is server-side → the device stays light (less RAM/power)
- Device: one HTTPS POST per wake — it reports its own state and gets the weather back

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
| BUSY | 20 | GPIO20 | D7 |

Battery sense — shown on the display and reported to the server:

| | GPIO | Super Mini pin | XIAO label |
|---|---|---|---|
| Divider tap | 3 | GPIO3 | D1 |

`BAT+ ──[1M]──┬──[1M]── GND`, tap to D1, plus `100nF` from tap to GND. ADC2 stops working once
Wi-Fi is on, so the tap has to sit on ADC1 (GPIO0–4); GPIO2 is a strapping pin that reads low
through the divider at boot, which is why BUSY moved off GPIO3 to make room.

- Super Mini labels pins by GPIO number; XIAO uses `D0–D10` (board positions mapped to different GPIOs)
- ⚠️ The C3's default SPI pins (SCK=4 / MISO=5) collide with RST(4)/DC(5) → `display.cpp` remaps SPI and re-asserts the pins
- RST and BUSY both stay wired. GxEPD2 accepts `-1` for either, falling back to a software reset
  and to fixed delays; tried here, and the display did not render correctly. Cause not investigated

## Setup (Wi-Fi)
Nothing to edit before building — the board asks for Wi-Fi itself. On a board with no stored
network it puts up an access point and shows the instructions on the e-Paper:

1. Join the Wi-Fi network **XIAO-weather** from a phone
2. Open **192.168.4.1** in a browser and pick your network

Credentials go into the Wi-Fi driver's own NVS namespace, so they survive firmware uploads.

To change networks later: the board offers the page again after five failed connections, and
pressing **RESET five times in a row** — faster than a connect takes — reaches the same point
immediately. Five minutes with nobody configuring anything and it goes back to sleep, keeping
whatever was stored.

## Build & Upload
```bash
pio run -t upload
pio device monitor -b 115200
```
- After flashing the deep-sleep firmware, re-uploads need download mode: hold **BOOT**, tap **RESET**

## Layout
```
src/
├─ common/          shared by every build target
│  ├─ config.h         all settings (URLs · pins · intervals)
│  ├─ net.h/.cpp       Wi-Fi connect + HTTPS GET/POST
│  ├─ battery.h/.cpp   battery voltage through the divider
│  └─ provision.h/.cpp the Wi-Fi setup portal
├─ weather/         the e-Paper display build
│  ├─ display.h/.cpp   e-Paper init + rendering (globals kept static = encapsulated)
│  ├─ weather_icons.h  weather icon bitmaps
│  └─ main.cpp         flow only (setup/loop)
└─ led/             the LED-only build
```

## Design notes
- [Persistence & refresh](docs/persistence-and-refresh.md) — why the last weather is kept in NVS
  (not RTC memory), and why a partial refresh of just the status line doesn't work here
- [JTAG debugging](docs/jtag-debug.md) — step debugging over the built-in USB-JTAG: the OpenOCD
  overrides it needs, and why download mode silently breaks it
- [Storing state in NVS](docs/nvs-internals.md) — why NVS over RTC memory, what NVS is, the
  `Preferences` API, and what a `putString()` becomes on flash, read back from a device dump
- [e-Paper power gating](docs/epd-power-gating.md) — switching the module's VCC from a GPIO so it
  stops drawing in deep sleep: the wiring, the pin choice, and why it is parked rather than merged
- [Low-voltage cutoff](docs/battery-cutoff.md) — a P-MOSFET that disconnects the battery outright
  at 3.4 V and comes back when a charger is plugged in, with no button and no standby current

## Server (MCP)
The weather/LED backend is a companion project: **[seung-gu/emcp](https://github.com/seung-gu/emcp)**.

- The device only hits a plain-text HTTPS endpoint
- The same server is exposed over MCP, so an AI client (e.g. ChatGPT) can query and control it
