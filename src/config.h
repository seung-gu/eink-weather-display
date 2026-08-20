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
#define EPD_BUSY   3
#define EPD_SCK    6
#define EPD_MOSI   7

// ==== Polling intervals ====
#define LED_POLL_MS      1000
#define WEATHER_POLL_MS  600000UL    // 10 min
