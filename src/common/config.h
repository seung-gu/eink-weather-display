#pragma once

// ==== MCP server ====
#define LED_URL       "https://emcp-led.fly.dev/led"
#define WEATHER_URL   "https://emcp-led.fly.dev/weather"
// Weather response (up to 7 lines): city / temp / condition / wind / humidity / high-low / precip%

// ==== Wi-Fi setup portal ====
#define AP_NAME        "XIAO-ESP32-C3"    // the access point the board puts up when it has no network
#define PORTAL_MINUTES 5
// Attempts that did not reach the network before the board offers the setup page again. The
// counter is bumped at the top of every boot and only cleared on a successful connect, so five
// wakes with the router gone gets you there — and so does pressing RESET five times in a row,
// faster than a connect takes.
#define WIFI_FAIL_LIMIT 5

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

// ==== Polling intervals ====
#define LED_POLL_MS      1000
#define WEATHER_POLL_MS  600000UL    // 10 min
