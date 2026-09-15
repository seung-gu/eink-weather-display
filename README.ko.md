[English](README.md) | [한국어](README.ko.md)

# eink-weather-display

**ESP32-C3 + 1.54" e-Paper 저전력 날씨 표시기.**
서버(MCP)에서 정제된 날씨를 HTTP로 수신 → e-ink 렌더링 → 딥슬립. 배터리로 수개월.

> Topics: `esp32-c3` · `e-ink` · `mcp` · `battery`

## 동작 흐름 (5조각)
![동작 흐름](docs/runtime.png)

- 모든 작업 `setup()`, `loop()` 비움 (웨이크 = 리셋)
- e-ink 쌍안정 → 화면 유지 전류 0, 갱신 순간만 소비
- 잠들어도 남아야 하는 상태는 RTC 메모리가 아니라 NVS에 둔다 (설계 노트 참고)

## 데이터 파이프라인
![데이터 파이프라인](docs/pipeline.png)

- 정제 = 서버 담당 → 기기 경량 (RAM·전력 절약)
- 기기: wake마다 HTTPS POST 하나 — 자기 상태를 보고하고 날씨를 응답으로 받는다

## 하드웨어
| 부품 | 사용 |
|---|---|
| MCU | ESP32-C3 (XIAO ESP32-C3 / Super Mini) |
| 디스플레이 | Waveshare 1.54" e-Paper (SSD1681, 200×200, 흑백) |
| 프레임워크 | Arduino (PlatformIO) |
| 전원 | LiPo — 딥슬립으로 수개월 |

## 배선 (`src/config.h`)
두 보드 **GPIO 번호 동일**(코드 같음) — 물리 핀 라벨만 다름.

| e-Paper | GPIO | Super Mini 핀 | XIAO 라벨 |
|---|---|---|---|
| VCC | — | 3V3 | 3V3 |
| GND | — | GND | GND |
| DIN (MOSI) | 7 | GPIO7 | D5 |
| CLK (SCK) | 6 | GPIO6 | D4 |
| CS | 10 | GPIO10 | D10 |
| DC | 5 | GPIO5 | D3 |
| RST | 4 | GPIO4 | D2 |
| BUSY | 20 | GPIO20 | D7 |

배터리 전압 측정 — 화면에 표시되고 서버로도 전송된다:

| | GPIO | Super Mini 핀 | XIAO 라벨 |
|---|---|---|---|
| 분압 탭 | 3 | GPIO3 | D1 |

`BAT+ ──[1M]──┬──[1M]── GND`, 가운데 탭을 D1로, 탭에서 `100nF`을 GND로. Wi-Fi가 켜지면 ADC2를
못 쓰므로 탭은 ADC1(GPIO0~4)에 있어야 하고, GPIO2는 스트래핑 핀이라 분압을 물리면 부팅 시 LOW로
읽힌다 — 그래서 BUSY를 GPIO3에서 비켜냈다.

- Super Mini는 핀에 GPIO 번호 직접 표기, XIAO는 `D0–D10`(보드 위치 이름, GPIO와 매핑 다름)
- ⚠️ C3 기본 SPI핀(SCK=4·MISO=5) ↔ RST(4)/DC(5) 충돌 → `display.cpp`에서 SPI 재지정 후 핀 재확정
- RST와 BUSY 모두 연결한 채로 둔다. GxEPD2는 둘 다 `-1`을 받아 소프트웨어 리셋과 고정 대기로
  대체할 수 있지만, 시도해보니 화면이 제대로 나오지 않았다. 원인은 확인하지 않았다

## 설정 (Wi-Fi)
빌드 전에 고칠 게 없다 — 보드가 직접 Wi-Fi를 물어본다. 저장된 네트워크가 없으면 AP를 띄우고
e-Paper에 안내를 표시한다:

1. 휴대폰에서 **XIAO-weather** 네트워크에 접속
2. 브라우저로 **192.168.4.1** 을 열고 쓰는 네트워크를 고른다

자격증명은 Wi-Fi 드라이버 전용 NVS 네임스페이스에 저장되므로 펌웨어를 다시 올려도 남는다.

나중에 바꾸려면: 접속에 다섯 번 연속 실패하면 보드가 알아서 이 화면을 다시 띄우고,
**RESET을 다섯 번 연타**하면(접속이 끝나기 전에 눌러야 한다) 즉시 같은 상태가 된다. 5분 동안
아무도 설정하지 않으면 저장된 값을 그대로 둔 채 다시 잠든다.

## 빌드 & 업로드
```bash
pio run -t upload
pio device monitor -b 115200
```
- 딥슬립 펌웨어 업로드 후 재업로드 = BOOT 누른 채 RESET (다운로드 모드)

## 구조
```
src/
├─ common/          모든 빌드가 공유
│  ├─ config.h         설정 (URL · 핀 · 주기)
│  ├─ net.h/.cpp       WiFi 연결 + HTTPS GET/POST
│  ├─ battery.h/.cpp   분압을 통한 배터리 전압
│  └─ provision.h/.cpp Wi-Fi 설정 포털
├─ weather/         e-Paper 표시기 빌드
│  ├─ display.h/.cpp   e-Paper 초기화 + 렌더링 (전역 객체 static 캡슐화)
│  ├─ weather_icons.h  날씨 아이콘 비트맵
│  └─ main.cpp         흐름 (setup/loop)
└─ led/             LED 전용 빌드
```

## 참고 (스터디)
- 딥슬립 = 리셋 → `setup()` 재시작, `loop()` 미사용
- SPI 충돌 회피: `SPI.begin(6, -1, 7, 10)` 후 핀 OUTPUT 재확정 + 수동 리셋
- 날씨 조건 문자열(`뇌우`·`눈`·`비`…) = 서버 응답 매칭용, 코드에 한국어 유지

## 설계 노트
- [저장과 화면 갱신](docs/persistence-and-refresh.ko.md) — 마지막 날씨를 NVS에 두는 이유(RTC 미사용),
  상태줄만 부분 갱신이 안 되는 이유
- [JTAG 디버깅](docs/jtag-debug.ko.md) — 내장 USB-JTAG 스텝 디버깅: 필요한 OpenOCD 설정,
  다운로드 모드에서 디버깅이 안 되는 이유
- [NVS에 상태 저장하기](docs/nvs-internals.ko.md) — RTC 대신 NVS인 이유, NVS가 무엇인지,
  `Preferences` API, 그리고 `putString()`이 플래시에서 어떤 모습이 되는지 (기기 덤프 기반)
- [e-Paper 전원 게이팅](docs/epd-power-gating.ko.md) — 모듈 VCC를 GPIO로 끊어 딥슬립 중 소비를
  없애는 방법: 배선, 핀 선택, 그리고 병합하지 않고 보류한 이유
- [저전압 차단](docs/battery-cutoff.ko.md) — 3.4V에서 P-MOSFET으로 배터리를 아예 떼어내고
  충전기를 꽂으면 돌아오는 회로. 버튼도 대기전류도 없다

## 서버 (MCP)
날씨·LED 백엔드 = 별도 프로젝트: **[seung-gu/emcp](https://github.com/seung-gu/emcp)**

- 기기: 평문 HTTPS 엔드포인트만
- 동일 서버 MCP 노출 → AI(ChatGPT 등) 조회·제어
