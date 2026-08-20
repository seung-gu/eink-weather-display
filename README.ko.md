[English](README.md) | [한국어](README.ko.md)

# eink-weather-display

**ESP32-C3 + 1.54" e-Paper**로 만든 저전력 날씨 표시기.
서버(MCP)에서 정제된 날씨를 HTTP로 받아 e-ink에 그리고, 딥슬립으로 배터리 수명을 늘린다.

> Topics: `esp32-c3` · `e-ink` · `mcp` · `battery`

## 하드웨어
| 부품 | 사용 |
|---|---|
| MCU | ESP32-C3 (Seeed XIAO ESP32-C3 또는 ESP32-C3 Super Mini) |
| 디스플레이 | Waveshare 1.54" e-Paper (SSD1681, 200×200, 흑백) |
| 프레임워크 | Arduino (PlatformIO) |
| 전원 | LiPo 배터리 — 딥슬립으로 수개월 |

## 배선 (ESP32-C3 Super Mini · `src/config.h`)
| e-Paper | ESP32-C3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| DIN (MOSI) | GPIO7 |
| CLK (SCK) | GPIO6 |
| CS | GPIO10 |
| DC | GPIO5 |
| RST | GPIO4 |
| BUSY | GPIO3 |

> XIAO ESP32-C3는 핀 라벨(D0–D10)이 달라 물리 위치만 바뀐다. GPIO 번호는 `config.h`에서 조정.
> ⚠️ C3 기본 SPI핀(SCK=4·MISO=5)이 RST(4)/DC(5)와 겹쳐서, `display.cpp`에서 SPI 재지정 후 핀을 재확정한다.

## 설정 (secrets)
WiFi 값은 `secrets.h`에 넣으며, 이 파일은 git에 올라가지 않는다. 클론 후:
```bash
cp src/secrets.example.h src/secrets.h   # 그리고 WIFI_SSID / WIFI_PASSWORD 채우기
```

## 빌드 & 업로드
```bash
pio run -t upload
pio device monitor -b 115200
```
> 딥슬립 펌웨어를 올린 뒤엔 보드가 자므로, 재업로드는 **BOOT 누른 채 RESET**(다운로드 모드)로.

## 구조
```
src/
├─ config.h         설정 (서버 URL · 핀 · 주기)
├─ net.h / .cpp     WiFi 연결 + HTTP GET
├─ display.h / .cpp e-Paper 초기화 + 날씨 렌더링
├─ weather_icons.h  날씨 아이콘 비트맵
└─ main.cpp         setup/loop (딥슬립 흐름)
```

## 동작 (5조각)
```
깨어남 → WiFi 접속 → 서버에서 받기(HTTP GET) → e-Paper 그리기 → deep sleep → 반복
```
- e-ink는 전원이 없어도 화면을 유지 → 자는 동안 소비 ≈ 0
- 딥슬립 웨이크 = 전체 리셋 → `setup()`부터 재시작
- 데이터 정제는 서버(MCP)가 맡아 기기는 가볍게 유지

## 코드 설명 (스터디용)
각 파일이 한 가지 역할만 맡도록 나눴다:
- **`config.h`** — 서버 URL·핀·주기 등 모든 설정을 한곳에. WiFi 값은 `secrets.h`로 분리.
- **`net.*`** — WiFi 연결(`connectWiFi`)과 HTTPS GET(`httpGet`). 네트워크 세부를 함수 뒤로 숨김.
- **`display.*`** — e-Paper 초기화(`displayBegin`)와 날씨 렌더링(`displayWeather`). `display`·`u8g2Fonts` 객체는 `static`으로 파일 내부에만 두어 캡슐화.
- **`main.cpp`** — 흐름만. `setup()`에서 5조각을 순서대로 실행하고 딥슬립.

포인트:
- **딥슬립 = 리셋**이라 모든 일을 `setup()`에서 하고 `loop()`는 비워둔다. 값을 재부팅 넘어 유지하려면 `RTC_DATA_ATTR`.
- **SPI 핀 충돌**: C3 하드웨어 SPI 기본핀(SCK=4·MISO=5)이 우리 RST(4)/DC(5)와 겹쳐서, `display.cpp`에서 `SPI.begin()`으로 6·7로 재지정한 뒤 핀을 다시 OUTPUT으로 확정 + 수동 리셋한다.
- **날씨 조건 문자열**(`뇌우`·`눈`·`비`…)은 서버 응답과 매칭돼야 해서 코드에 한국어로 유지한다.

## 서버 (MCP)
- 기기는 단순 HTTPS GET 엔드포인트만 사용
- 같은 서버가 MCP로도 열려 있어, ChatGPT 등 AI에서 데이터 분석·제어 가능
