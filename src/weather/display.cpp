#include "display.h"
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "weather_icons.h"
#include "config.h"

// Globals used only within this file (static = not visible elsewhere = encapsulation)
static GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
static U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

void displayBegin() {
  display.init(115200);
  SPI.end();
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  // NOTE: the C3's default SPI pins (SCK=4, MISO=5) collide with RST(4)/DC(5)
  //       -> after remapping SPI, re-assert the pins as outputs + manual reset
  pinMode(EPD_CS, OUTPUT);  digitalWrite(EPD_CS, HIGH);
  pinMode(EPD_DC, OUTPUT);  digitalWrite(EPD_DC, HIGH);
  pinMode(EPD_RST, OUTPUT);
  digitalWrite(EPD_RST, HIGH); delay(20);
  digitalWrite(EPD_RST, LOW);  delay(20);
  digitalWrite(EPD_RST, HIGH); delay(50);
  u8g2Fonts.begin(display);
}

static void drawCentered(const String& s, int y) {
  int w = u8g2Fonts.getUTF8Width(s.c_str());
  u8g2Fonts.setCursor((display.width() - w) / 2, y);
  u8g2Fonts.print(s);
}

// Condition string -> 48x48 bitmap icon (cx,cy = center).
// Keys stay in Korean because they match the server's Korean weather text.
static void drawWeatherIcon(const String& c, int cx, int cy) {
  const unsigned char* icon;
  if      (c.indexOf("뇌우")   >= 0) icon = icon_storm;
  else if (c.indexOf("눈")     >= 0) icon = icon_snow;
  else if (c.indexOf("소나기") >= 0) icon = icon_showers;
  else if (c.indexOf("이슬비") >= 0) icon = icon_drizzle;
  else if (c.indexOf("비")     >= 0) icon = icon_rain;
  else if (c.indexOf("안개")   >= 0) icon = icon_fog;
  else if (c.indexOf("흐림")   >= 0) icon = icon_cloudy;
  else if (c.indexOf("구름")   >= 0) icon = icon_partly;
  else if (c.indexOf("맑음")   >= 0) icon = icon_clear;
  else                               icon = icon_cloudy;
  display.drawBitmap(cx - WI_W / 2, cy - WI_H / 2, icon, WI_W, WI_H, GxEPD_BLACK);
}

// High/low: "22°/12°" -> up-triangle 22°  down-triangle 12° (triangles drawn manually), baseline = cy
static void drawHighLow(const String& hl, int cy) {
  int sl = hl.indexOf('/');
  String hi = (sl < 0) ? hl : hl.substring(0, sl);
  String lo = (sl < 0) ? "" : hl.substring(sl + 1);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  int wh = u8g2Fonts.getUTF8Width(hi.c_str());
  int wl = u8g2Fonts.getUTF8Width(lo.c_str());
  const int tri = 9, pad = 3, mid = 14;
  int total = tri + pad + wh + mid + tri + pad + wl;
  int x = (display.width() - total) / 2;
  display.fillTriangle(x, cy, x + tri, cy, x + tri / 2, cy - 10, GxEPD_BLACK);  // up triangle
  u8g2Fonts.setCursor(x + tri + pad, cy); u8g2Fonts.print(hi);
  x += tri + pad + wh + mid;
  display.fillTriangle(x, cy - 10, x + tri, cy - 10, x + tri / 2, cy, GxEPD_BLACK);  // down triangle
  u8g2Fonts.setCursor(x + tri + pad, cy); u8g2Fonts.print(lo);
}

// One bottom stat cell: icon + value
static void drawStat(const unsigned char* icon, const String& val, int ix, int tx, int iy, int ty) {
  if (!val.length()) return;
  display.drawBitmap(ix, iy, icon, WI_S, WI_S, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  u8g2Fonts.setCursor(tx, ty);
  u8g2Fonts.print(val);
}

// RSSI (dBm) -> 0..4 signal level. Real RSSI is negative; >= 0 means "no signal".
static int rssiLevel(int rssi) {
  if (rssi >= 0)   return 0;
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

// Signal gauge: 4 ascending bars, filled up to the level, outline beyond it.
// baseline = bottom of the bars.
static void drawSignalGauge(int rssi, int x, int baseline) {
  int bars = rssiLevel(rssi);
  for (int i = 0; i < 4; i++) {
    int h  = 3 + i * 3;                 // ascending heights: 3,6,9,12
    int bx = x + i * 5;
    int by = baseline - h;
    if (i < bars) display.fillRect(bx, by, 3, h, GxEPD_BLACK);   // filled
    else          display.drawRect(bx, by, 3, h, GxEPD_BLACK);   // outline
  }
}

// Bottom line: Wi-Fi connect time (left) + signal gauge (right)
static void drawBottomLine(uint32_t wifiMs, int rssi) {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  u8g2Fonts.setCursor(6, display.height() - 4);
  if (wifiMs) u8g2Fonts.printf("wifi %u ms", wifiMs);
  else        u8g2Fonts.print("offline");
  drawSignalGauge(rssi, 172, display.height() - 4);
}

// Response -> screen. Call only when it changed.
void displayWeather(const String& w, uint32_t wifiMs, int rssi) {
  String p[7];
  int idx = 0, start = 0;
  for (int i = 0; i <= (int)w.length() && idx < 7; i++) {
    if (i == (int)w.length() || w[i] == '\n') { p[idx++] = w.substring(start, i); start = i + 1; }
  }
  for (int i = 0; i < 7; i++) p[i].trim();
  String city = p[0], temp = p[1], cond = p[2], wind = p[3], humid = p[4], hilo = p[5], pop = p[6];

  display.setRotation(1);
  u8g2Fonts.setFontMode(1);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawRect(0, 0, display.width(), display.height(), GxEPD_BLACK);
    const int YO = 12;                               // nudge everything down a bit (tunable)
    u8g2Fonts.setFont(u8g2_font_unifont_t_korean2);
    drawCentered(city, 14 + YO);
    if (cond.length()) drawWeatherIcon(cond, 100, 45 + YO);   // icon 48 (skip when no data)
    u8g2Fonts.setFont(u8g2_font_helvB18_tf);
    drawCentered(temp, 92 + YO);                      // current temp
    u8g2Fonts.setFont(u8g2_font_unifont_t_korean2);
    drawCentered(cond, 110 + YO);                     // condition
    if (hilo.length()) drawHighLow(hilo, 128 + YO);   // high/low
    // Bottom stats: wind / humidity / precipitation
    if (wind.length() || humid.length() || pop.length()) {
      display.drawLine(12, 137 + YO, 188, 137 + YO, GxEPD_BLACK);
      drawStat(icon_wind,     wind,  6,  28, 143 + YO, 158 + YO);
      drawStat(icon_humidity, humid, 82, 104, 143 + YO, 158 + YO);
      drawStat(icon_umbrella, pop,   142, 164, 143 + YO, 158 + YO);
    }
    drawBottomLine(wifiMs, rssi);          // Wi-Fi time + signal icon
  } while (display.nextPage());
  display.hibernate();
}

