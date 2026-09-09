#pragma once

#include "secrets.h"   // WIFI_SSID / WIFI_PASSWORD (git-ignored — copy secrets.example.h and fill in)

// ==== MCP server ====
#define LED_URL       "https://emcp-led.fly.dev/led"
#define WEATHER_URL   "https://emcp-led.fly.dev/weather"
// Weather response (up to 7 lines): city / temp / condition / wind / humidity / high-low / precip%

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
