#pragma once

// ==== Firmware build ====
// Goes out with every report so a change in the numbers can be pinned to a build — the battery
// figures only mean something next to what was running when they were taken. Normally set by
// scripts/fw_version.py at compile time; this is what you get building outside a checkout.
#ifndef FW_VERSION
#define FW_VERSION "unknown"
#endif
// The commit FW_VERSION was built from, with -dirty when the tree had changes. FW_VERSION is the
// release tag on a release build; this is what updates compare, since the server knows commits.
#ifndef FW_COMMIT
#define FW_COMMIT "unknown"
#endif
// Which build it is, as the platformio.ini env name (weather-xiao, ...). The server picks the update
// file by it. Also set by scripts/fw_version.py.
#ifndef FW_BOARD
#define FW_BOARD "unknown"
#endif

// ==== MCP server ====
#define LED_URL       "https://emcp-led.fly.dev/led"
#define SERVER_HOST   "emcp.fly.dev"
#define WEATHER_URL   "https://" SERVER_HOST "/weather"
// The registration QR opens this with the claim code on the end, which lands on the dashboard
// with the code filled in. Lower case keeps it inside the 32 bytes a version-2 QR holds.
#define REGISTER_URL  "https://" SERVER_HOST "/r/"
// Weather response: city / cond / wind_kmh / pop / stamp, and out_temp_c / out_humidity /
// out_temp_max_c / out_temp_min_c. Numbers arrive as numbers; the units are a display decision
// and go on here. The server also sends the old names (temp_c, ...) for boards not yet reflashed.

// ==== Registration ====
// Only once Wi-Fi works: a board the server does not know asks once, shows a code for its owner
// to scan or type, and sleeps with no wake timer. The owner registers on the dashboard and
// presses RESET, and that wake asks again. Nothing keeps checking in between.

// ==== Wake schedule ====
#define SLEEP_MINUTES 30
// After a wake that could not reach the network, come back sooner. A router reboot is over in
// a minute or two, and WIFI_FAIL_LIMIT caps the number of failed wakes either way, so this
// halves the time spent stale without costing anything.
#define RETRY_MINUTES 5

// ==== Wi-Fi setup portal ====
#define AP_NAME        "XIAO-ESP32-C3"    // the access point the board puts up when it has no network
#define PORTAL_MINUTES 5
// Attempts that did not reach the network before the board offers the setup page again. The
// counter is bumped at the top of every boot and only cleared on a successful connect, so five
// wakes with the router gone gets you there — and so does pressing RESET five times in a row,
// faster than a connect takes.
#define WIFI_FAIL_LIMIT 5

// ==== Failure log ====
// Wakes that never reached the server pile up here until one does. Wi-Fi failures stop
// themselves at WIFI_FAIL_LIMIT, but HTTP failures never touch that counter — a live router
// with a dead uplink clears it on every wake and retries every RETRY_MINUTES with nothing to
// stop it. NVS refuses a string past 4000 bytes without saying so, so stop well short.
#define LOG_MAX_BYTES 2048

// ==== Onboard LED (GPIO8, active-LOW) ====
#define LED_PIN 8

// ==== e-Paper (Waveshare 1.54" SSD1681) wiring ====
#define EPD_CS    10
#define EPD_DC     5
#define EPD_RST    4
#define EPD_BUSY  20    // moved off GPIO3 to free an ADC1 channel for BAT_ADC
#define EPD_SCK    6
#define EPD_MOSI   7

// ==== Room sensor (SHT40, I2C) ====
// The pins XIAO prints as SDA/SCL are taken by the e-Paper's SPI, so the bus moves. GPIO2 is a
// strapping pin and has to be high at boot, which the bus pull-ups already do.
// SDA stays off GPIO21 (UART0 TX, which the ROM drives at every reset) and off GPIO9: that one
// picks the boot mode, so a bus left stuck with SDA low would boot to download mode instead of
// the app and never wake again. GPIO8 is also a strapping pin but only matters when GPIO9 is
// low, so a stuck line there still boots normally.
#define SHT_SDA    8
#define SHT_SCL    2

// ==== Battery sense ====
// 1:1 divider from BAT+ (2x 1M + 100nF). ADC2 is unusable while Wi-Fi is on, so this
// has to be an ADC1 pin (GPIO0-4); GPIO2 is a strapping pin and reads low through the
// divider at boot, which leaves GPIO3 as the only workable choice.
#define BAT_ADC    3

// ==== Polling interval (the LED build stays awake and polls) ====
#define LED_POLL_MS 1000
