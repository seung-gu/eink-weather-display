#pragma once

// ==== Firmware build ====
// Goes out with every report so a change in the numbers can be pinned to a build. Bump it by
// hand when flashing something whose effect you want to see in the data — the battery figures
// only mean anything next to what was running when they were taken.
#define FW_VERSION "2026-09-16"

// ==== MCP server ====
#define LED_URL       "https://emcp-led.fly.dev/led"
#define WEATHER_URL   "https://emcp-led.fly.dev/weather"
// Weather response: city / temp_c / cond / wind_kmh / humidity / temp_max_c / temp_min_c /
// pop / stamp. Numbers arrive as numbers; the units are a display decision and go on here.

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
#define LOG_MAX_BYTES 1024

// ==== Onboard LED (GPIO8, active-LOW) ====
#define LED_PIN 8

// ==== e-Paper (Waveshare 1.54" SSD1681) wiring ====
#define EPD_CS    10
#define EPD_DC     5
#define EPD_RST    4
#define EPD_BUSY  20    // moved off GPIO3 to free an ADC1 channel for BAT_ADC
#define EPD_SCK    6
#define EPD_MOSI   7

// ==== Battery sense ====
// 1:1 divider from BAT+ (2x 1M + 100nF). ADC2 is unusable while Wi-Fi is on, so this
// has to be an ADC1 pin (GPIO0-4); GPIO2 is a strapping pin and reads low through the
// divider at boot, which leaves GPIO3 as the only workable choice.
#define BAT_ADC    3

// ==== Polling interval (the LED build stays awake and polls) ====
#define LED_POLL_MS 1000
