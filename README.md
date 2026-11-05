[English](README.md) | [한국어](docs/ko/README.md)

# eink-weather-display

![ESP32-C3](https://img.shields.io/badge/ESP32--C3-XIAO-E7352C?logo=espressif&logoColor=white)
![Arduino core](https://img.shields.io/badge/Arduino_core-2.0.17-00878F?logo=arduino&logoColor=white)
![PlatformIO](https://img.shields.io/badge/PlatformIO-espressif32_7.0.1-F5822A?logo=platformio&logoColor=white)
![C++14](https://img.shields.io/badge/C%2B%2B-14-00599C?logo=cplusplus&logoColor=white)
<br>
![e-Paper](https://img.shields.io/badge/e--Paper-Waveshare_1.54%22-333333)
![SHT40](https://img.shields.io/badge/SHT40-I2C-4B5563)
![GxEPD2](https://img.shields.io/badge/GxEPD2-1.6.9-4B5563)
![ArduinoJson](https://img.shields.io/badge/ArduinoJson-7.4.3-4B5563)
![WiFiManager](https://img.shields.io/badge/WiFiManager-2.0.17-4B5563)
<br>
[![emcp](https://img.shields.io/badge/server-emcp-14B8A6?logo=modelcontextprotocol&logoColor=white)](https://github.com/seung-gu/emcp)
![Fly.io](https://img.shields.io/badge/Fly.io-emcp.fly.dev-7B3BE2?logo=flydotio&logoColor=white)
[![GitHub Actions](https://img.shields.io/badge/GitHub_Actions-release-2088FF?logo=githubactions&logoColor=white)](https://github.com/seung-gu/eink-weather-display/blob/main/.github/workflows/release.yml)

<p>
  <img src="docs/images/photo-front.jpg" width="49%" alt="The board up close on the fridge">
  <img src="docs/images/photo-sensor.jpg" width="49%" alt="The SHT40 sensor at the opening in the case">
</p>

A low-power weather display: an ESP32-C3 driving a 1.54" e-Paper. Every 30 minutes it wakes, draws the weather and the room's temperature and humidity, and goes back to deep sleep. It runs for months on a battery.

## Features
| Feature | What it does |
|---|---|
| Weather | Temperature, high and low, conditions, wind, humidity and chance of rain for the board's location, plus room temperature and humidity, battery and Wi-Fi signal |
| Low power | Awake only to refresh, deep sleep the rest of the time. The e-Paper holds its image without power |
| History | Every wake stores the battery, Wi-Fi, room readings and the weather it got on the server, shown as graphs on a dashboard |
| AI (MCP) | From the Claude or ChatGPT app: register a board, change its weather location, rename it, read and analyse the last 7 days of reports |
| Updates | A new release reaches every board over Wi-Fi on its next wake, and a board goes back to its previous firmware if the new one fails |
| Setup | No code to edit: Wi-Fi is set from a phone, and the QR on the screen registers the board to your account |

## How it works
![Wake, measure, talk to the server, draw, update the firmware if there is a newer one, deep sleep](docs/images/cycle.png)

- One HTTPS request per wake carries the report out and the weather back
- The server formats the weather, so the board only draws what it gets and stays light on RAM and power

## The emcp server
![The board, Claude or ChatGPT, a browser and GitHub Actions call the emcp server; the server calls open-meteo, Neon and GitHub](docs/images/emcp.png)

![The dashboard: 20 days of battery, Wi-Fi signal, awake time, room temperature and humidity](docs/images/dashboard-en.png)

The backend is a separate project, [seung-gu/emcp](https://github.com/seung-gu/emcp), running on Fly.io. It looks up the weather, stores the reports, and serves the dashboard, the MCP tools and the firmware.

## Firmware updates
![GitHub Actions builds a release, the emcp server holds it, and the board installs it on its next wake; kept if it works, rolled back if not](docs/images/ota.png)

How to publish one, and the rules a board follows: [Firmware layout and updates](docs/en/firmware.md).

## Hardware
| Part | Used |
|---|---|
| MCU | Seeed XIAO ESP32-C3 |
| Display | Waveshare 1.54" e-Paper V2.1 (200×200, black/white) |
| Room sensor | SHT40 (I2C, optional) |
| Power | LiPo battery |

Pins and circuits: [Hardware and wiring](docs/en/hardware.md).

## Getting started
1. Build and upload: `pio run -e weather-xiao -t upload`
2. Wi-Fi: join `XIAO-ESP32-C3` from a phone, open `192.168.4.1` and pick your network
3. Register: scan the QR on the screen and register the board on the dashboard (or give the code to Claude or ChatGPT), then press RESET on the board

Changing networks and upload trouble: [Setup and registration](docs/en/setup.md).

## Docs
| Doc | About |
|---|---|
| [Hardware and wiring](docs/en/hardware.md) | Pin connections, the battery divider, why each pin |
| [Setup and registration](docs/en/setup.md) | Changing Wi-Fi, how registration works, uploading in download mode |
| [Firmware layout and updates](docs/en/firmware.md) | Code layout, one wake in detail, update rules |
| [Registering a board over QR](docs/en/registration.md) | Token and claim code, what each server answer does, the QR screen |
| [Persistence & refresh](docs/en/persistence-and-refresh.md) | Why the last weather lives in NVS, how a partial refresh survives deep sleep |
| [Storing state in NVS](docs/en/nvs-internals.md) | How NVS is laid out, and the `Preferences` API |
| [JTAG debugging](docs/en/jtag-debug.md) | Step debugging over the built-in USB-JTAG |
| [e-Paper power gating](docs/en/epd-power-gating.md) | Switching the module's VCC from a GPIO (parked) |
| [Low-voltage cutoff](docs/en/battery-cutoff.md) | A P-MOSFET that disconnects the battery at 3.4 V |
| [Battery leakage current troubleshooting](docs/en/deep-sleep-drain.md) | Why RST on the V2.1 board has to stay low in deep sleep |

## Changelog
[CHANGELOG.md](CHANGELOG.md).
