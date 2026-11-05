#include "display.h"
#include <SPI.h>
#include "driver/gpio.h"
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "qrcode.h"                 // ESP-IDF's QR encoder, shipped with the core
#include "weather_icons.h"
#include "config.h"

// Globals used only within this file (static = not visible elsewhere = encapsulation)
static GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
static U8G2_FOR_ADAFRUIT_GFX u8g2Fonts;

// Every line into the panel. Each floats through deep sleep unless held by itself: GPIO0-5 are
// never held, and gpio_deep_sleep_hold_en() cuts GPIO6-21 off. On the Waveshare V2.1 board RST
// also switches the module's supply, through 1 kOhm into a transistor's base: held high, that is
// about 2.6 mA for as long as it is high.
static const gpio_num_t panelInputs[] = {
    (gpio_num_t)EPD_RST, (gpio_num_t)EPD_CS, (gpio_num_t)EPD_DC,
    (gpio_num_t)EPD_SCK, (gpio_num_t)EPD_MOSI};

// ---- Partial refresh ----
// A partial refresh drives only the pixels that differ between the panel's two RAMs, so 0x26 must
// hold exactly what the screen shows. Deep sleep switches the module off and that RAM with it, so
// the frame each wake draws is kept here, in RTC memory, and written back to 0x26 on the next.
// Drawing goes to a canvas because GxEPD2 keeps its own buffer private.
static const int W = GxEPD2_154_D67::WIDTH, H = GxEPD2_154_D67::HEIGHT;
static GFXcanvas1 canvas(W, H);

// Testing showed no ghosting, with the city moved and every value and the icon changing; the
// full refresh every 12 hours is insurance. Counted in refreshes, so a run of RETRY_MINUTES
// wakes brings it forward.
#define FULL_EVERY (12 * 60 / SLEEP_MINUTES)

// A reset or a power cut empties RTC memory, so the first refresh after one is full.
RTC_DATA_ATTR static uint8_t lastFrame[W * H / 8];
RTC_DATA_ATTR static bool    frameKept = false;   // lastFrame is what the panel shows
RTC_DATA_ATTR static uint8_t partials  = 0;       // partial refreshes since the last full one

// Sends the canvas to the panel. Only a weather screen is kept as the base for a partial refresh,
// so the screen after a message or a QR is a full refresh.
static void showFrame(bool keep) {
  const uint8_t* next = canvas.getBuffer();
  if (keep && frameKept && partials < FULL_EVERY - 1) {
    Serial.println("partial refresh");
    display.epd2.writeImageToPrevious(lastFrame, 0, 0, W, H);
    display.epd2.writeImage(next, 0, 0, W, H);
    display.epd2.refresh(0, 0, W, H);
    partials++;
  } else {
    Serial.println("full refresh");
    display.epd2.writeImageForFullRefresh(next, 0, 0, W, H);
    display.epd2.refresh(false);
    partials = 0;
  }
  memcpy(lastFrame, next, sizeof lastFrame);
  frameKept = keep;
  display.hibernate();
}

void displayBegin() {
  // Still held from the last deep sleep (see displayHoldPins()), and a held pad ignores writes.
  for (gpio_num_t p : panelInputs) gpio_hold_dis(p);
  // Claimed with explicit pins before init(), because GxEPD2 calls SPI.begin() with no arguments
  // and that falls back to the variant's defaults: SCK=4/MISO=5 on the Super Mini, which are
  // RST and DC, and SCK=8 on the XIAO, which is the sensor's SDA. Having begun the bus already
  // makes that call return early, so neither set is ever touched.
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  // false skips GxEPD2's forced first full refresh, and its clearing of both RAMs before the
  // first write, which showFrame() replaces by writing both itself.
  display.init(115200, !frameKept);
  // Kept from when the default mapping did take RST and DC: re-assert them, then reset the
  // module by hand. This is the sequence the display is known to render with.
  pinMode(EPD_CS, OUTPUT);  digitalWrite(EPD_CS, HIGH);
  pinMode(EPD_DC, OUTPUT);  digitalWrite(EPD_DC, HIGH);
  pinMode(EPD_RST, OUTPUT);
  digitalWrite(EPD_RST, HIGH); delay(20);
  digitalWrite(EPD_RST, LOW);  delay(20);
  digitalWrite(EPD_RST, HIGH); delay(50);
  u8g2Fonts.begin(canvas);
}

void displayHoldPins() {
  SPI.end();                                  // CLK and DIN back from the SPI peripheral to plain pins
  // All low. RST low switches the module off, and a line left high would feed the unpowered
  // module through the level shifter's clamp diodes.
  for (gpio_num_t p : panelInputs) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
    gpio_hold_en(p);
  }
}

static void drawCentered(const String& s, int y) {
  int w = u8g2Fonts.getUTF8Width(s.c_str());
  u8g2Fonts.setCursor((canvas.width() - w) / 2, y);
  u8g2Fonts.print(s);
}

static void drawCenteredAt(const String& s, int cx, int y) {
  int w = u8g2Fonts.getUTF8Width(s.c_str());
  u8g2Fonts.setCursor(cx - w / 2, y);
  u8g2Fonts.print(s);
}

// Where the QR goes. The encoder hands its result to a plain function pointer, so the position
// travels in these rather than as arguments.
static int qrLeft, qrTop, qrBox;

// Each module as a square, as large as fits in qrBox: a short URL is a 25-module QR, 4 px each.
static void drawQrModules(esp_qrcode_handle_t qr) {
  int n = esp_qrcode_get_size(qr);
  int px = qrBox / n;
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      if (esp_qrcode_get_module(qr, x, y))
        canvas.fillRect(qrLeft + x * px, qrTop + y * px, px, px, GxEPD_BLACK);
}

static void drawQr(const String& text, int left, int top, int box) {
  qrLeft = left; qrTop = top; qrBox = box;
  esp_qrcode_config_t cfg = {};
  cfg.display_func       = drawQrModules;
  cfg.max_qrcode_version = 3;                 // 29 modules: still 3 px each in a 100 px box
  cfg.qrcode_ecc_level   = ESP_QRCODE_ECC_LOW;  // a crisp screen up close needs little recovery
  esp_qrcode_generate(&cfg, text.c_str());
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
  canvas.drawBitmap(cx - WI_W / 2, cy - WI_H / 2, icon, WI_W, WI_H, GxEPD_BLACK);
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
  int x = (canvas.width() - total) / 2;
  canvas.fillTriangle(x, cy, x + tri, cy, x + tri / 2, cy - 10, GxEPD_BLACK);  // up triangle
  u8g2Fonts.setCursor(x + tri + pad, cy); u8g2Fonts.print(hi);
  x += tri + pad + wh + mid;
  canvas.fillTriangle(x, cy - 10, x + tri, cy - 10, x + tri / 2, cy, GxEPD_BLACK);  // down triangle
  u8g2Fonts.setCursor(x + tri + pad, cy); u8g2Fonts.print(lo);
}

// One bottom stat cell: icon + value
static void drawStat(const unsigned char* icon, const String& val, int ix, int tx, int iy, int ty) {
  if (!val.length()) return;
  canvas.drawBitmap(ix, iy, icon, WI_S, WI_S, GxEPD_BLACK);
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
    if (i < bars) canvas.fillRect(bx, by, 3, h, GxEPD_BLACK);   // filled
    else          canvas.drawRect(bx, by, 3, h, GxEPD_BLACK);   // outline
  }
}

// Battery voltage -> 0..4 bars. A lithium cell's voltage is not proportional to what is left in
// it — it falls fast from 4.2, sits near 3.8 for most of the discharge, then drops away — so the
// thresholds are spaced to match that rather than split the range evenly.
static int batteryBars(uint32_t mv) {
  if (mv >= 4000) return 4;
  if (mv >= 3850) return 3;
  if (mv >= 3750) return 2;
  if (mv >= 3650) return 1;
  return 0;
}

// Battery outline with a terminal nub, filled from the left in quarters. 22x11 including the nub.
static void drawBatteryIcon(uint32_t mv, int x, int top) {
  const int w = 20, h = 11;
  canvas.drawRect(x, top, w, h, GxEPD_BLACK);
  canvas.drawRect(x + w, top + 3, 3, 5, GxEPD_BLACK);           // terminal, hollow like the body
                                                                 // so only the charge reads as fill
  int level = batteryBars(mv);
  if (level) canvas.fillRect(x + 2, top + 2, (w - 4) * level / 4, h - 4, GxEPD_BLACK);
}

// Top row: when the weather was fetched on the left, the battery on the right, the city centred
// below both. A stored response carries the time it was fetched, so only show it on a wake that
// brought a new one. The time sits 2 px above the battery's baseline, which lines the two up on
// their centres instead of their bottoms.
static void drawStatusRow(const String& city, const String& clock, bool updated,
                          uint32_t batteryMv, int batteryBaseline, int cityBaseline) {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  u8g2Fonts.setCursor(6, batteryBaseline - 2);
  u8g2Fonts.print(updated ? clock : "offline");
  drawBatteryIcon(batteryMv, 170, batteryBaseline - 11);
  // Centred on the screen. Only a name wide enough to reach the battery gets pushed left, so the
  // usual ones land exactly where they always did rather than one pixel off.
  u8g2Fonts.setFont(u8g2_font_unifont_t_korean2);
  int w = u8g2Fonts.getUTF8Width(city.c_str());
  int x = (canvas.width() - w) / 2;
  if (x + w > 166) x = 166 - w;
  u8g2Fonts.setCursor(x, cityBaseline);
  u8g2Fonts.print(city);
}

// A roof over a room, drawn rather than stored: the stats icons are 20 px and this row is set in
// the small face, so a bitmap that size would tower over its own text.
static void drawHouse(int x, int top) {
  const int w = 13, h = 12;
  canvas.fillTriangle(x, top + 5, x + w / 2, top, x + w, top + 5, GxEPD_BLACK);
  canvas.drawRect(x + 2, top + 5, w - 4, h - 5, GxEPD_BLACK);
}

// Bottom row: this room on the left, the signal on the right. Set in the small face — this row
// is the aside, not the weather.
static void drawRoomRow(const Room& room, int rssi, int baseline) {
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  // Drawn whether or not the sensor answered: dashes say it went quiet, where an empty corner
  // would read as a board with no sensor on it.
  drawHouse(6, baseline - 11);
  u8g2Fonts.setCursor(24, baseline);
  if (room.ok) u8g2Fonts.printf("%.0f°C/%.0f%%", room.c, room.rh);
  else         u8g2Fonts.print("--°C/--%");
  drawSignalGauge(rssi, 174, baseline);
}

// Plain full-screen message, for the states that have no weather to show yet.
void displayMessage(const String& title, const String& body) {
  canvas.setRotation(1);
  u8g2Fonts.setFontMode(1);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  canvas.fillScreen(GxEPD_WHITE);
  canvas.drawRect(0, 0, canvas.width(), canvas.height(), GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB12_tf);
  drawCentered(title, 46);
  canvas.drawLine(20, 58, 180, 58, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  int y = 84;
  for (int start = 0; start <= (int)body.length() && y < canvas.height() - 8; y += 19) {
    int nl = body.indexOf('\n', start);
    if (nl < 0) nl = body.length();
    u8g2Fonts.setCursor(16, y);
    u8g2Fonts.print(body.substring(start, nl));
    if (nl == (int)body.length()) break;
    start = nl + 1;
  }
  showFrame(false);
}

// QR on the left, the code on the right, the steps underneath. The QR leads because scanning it
// fills the code in on the dashboard; the code is there for anyone who cannot scan. The phone is
// already on the internet by now: this screen only comes up once the board itself is online.
void displayRegister(const String& code) {
  canvas.setRotation(1);
  u8g2Fonts.setFontMode(1);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  canvas.fillScreen(GxEPD_WHITE);
  canvas.drawRect(0, 0, canvas.width(), canvas.height(), GxEPD_BLACK);
  drawQr(String(REGISTER_URL) + code, 8, 8, 100);

  const int cx = 156;                       // centre of the column right of the QR
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawCenteredAt("Scan to", cx, 22);
  drawCenteredAt("register", cx, 34);
  u8g2Fonts.setFont(u8g2_font_helvB14_tf);
  drawCenteredAt(code, cx, 66);
  u8g2Fonts.setFont(u8g2_font_helvB08_tf);
  drawCenteredAt("or enter it at", cx, 88);
  drawCenteredAt(SERVER_HOST, cx, 100);

  canvas.drawLine(12, 116, 188, 116, GxEPD_BLACK);
  u8g2Fonts.setFont(u8g2_font_helvB10_tf);
  u8g2Fonts.setCursor(12, 140); u8g2Fonts.print("1. Scan or enter");
  u8g2Fonts.setCursor(12, 159); u8g2Fonts.print("2. Log in, register");
  u8g2Fonts.setCursor(12, 178); u8g2Fonts.print("3. Press RESET");
  showFrame(false);
}

// Response -> screen. Call only when it changed.
void displayWeather(const String& w, bool updated, int rssi, uint32_t batteryMv,
                    const Room& room) {
  // The server sends numbers as numbers, so the units go on here — they are a display decision.
  // A lookup the server could not complete leaves the weather keys null, and so does an empty
  // NVS; both read back as empty strings, which the drawing code below already skips over.
  JsonDocument d;
  deserializeJson(d, w);
  String city  = d["city"]  | "";
  String cond  = d["cond"]  | "";
  String clock = d["stamp"] | "";
  // Outdoor values carry out_ so they cannot be mistaken for this room's.
  String temp  = d["out_temp_c"].isNull()     ? String() : String(d["out_temp_c"].as<int>())   + "°C";
  String wind  = d["wind_kmh"].isNull()       ? String() : String(d["wind_kmh"].as<int>())     + "km/h";
  String humid = d["out_humidity"].isNull()   ? String() : String(d["out_humidity"].as<int>()) + "%";
  String pop   = d["pop"].isNull()            ? String() : String(d["pop"].as<int>())          + "%";
  String hilo  = d["out_temp_max_c"].isNull() ? String()
               : String(d["out_temp_max_c"].as<int>()) + "°/" + String(d["out_temp_min_c"].as<int>()) + "°";

  canvas.setRotation(1);
  u8g2Fonts.setFontMode(1);
  u8g2Fonts.setForegroundColor(GxEPD_BLACK);
  u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
  canvas.fillScreen(GxEPD_WHITE);
  canvas.drawRect(0, 0, canvas.width(), canvas.height(), GxEPD_BLACK);
  const int YO = 12;                                // nudge everything down a bit (tunable)
  drawStatusRow(city, clock, updated, batteryMv, 18, 19 + YO);
  if (cond.length()) drawWeatherIcon(cond, 100, 45 + YO);   // icon 48 (skip when no data)
  u8g2Fonts.setFont(u8g2_font_helvB18_tf);
  drawCentered(temp, 91 + YO);                      // current temp
  u8g2Fonts.setFont(u8g2_font_unifont_t_korean2);
  drawCentered(cond, 109 + YO);                     // condition
  if (hilo.length()) drawHighLow(hilo, 127 + YO);   // high/low
  // Two rows of the same shape, split by the line: outdoors above, indoors below. Wind,
  // humidity and precipitation are what the server sent; the row under them is this room.
  canvas.drawLine(12, 133 + YO, 188, 133 + YO, GxEPD_BLACK);
  drawStat(icon_wind,     wind,  6,  30, 139 + YO, 154 + YO);
  drawStat(icon_humidity, humid, 82, 106, 139 + YO, 154 + YO);
  drawStat(icon_umbrella, pop,   142, 166, 139 + YO, 154 + YO);
  drawRoomRow(room, rssi, 177 + YO);
  showFrame(true);
}

