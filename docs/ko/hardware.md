[English](../en/hardware.md) | [한국어](hardware.md)

# 하드웨어와 배선

| 부품 | 사용 |
|---|---|
| MCU | Seeed XIAO ESP32-C3 |
| 디스플레이 | Waveshare 1.54" e-Paper V2.1 (SSD1681, 200×200, 흑백) |
| 실내 센서 | SHT40 브레이크아웃 (I2C, 없어도 동작) |
| 전원 | LiPo 배터리 |

모든 핀은 `src/common/config.h`에서 설정. XIAO의 핀 라벨 `D0–D10`은 보드 위 위치 이름이라 GPIO 번호와
다름.

## e-Paper
| e-Paper | GPIO | XIAO 라벨 |
|---|---|---|
| VCC | — | 3V3 |
| GND | — | GND |
| DIN (MOSI) | 7 | D5 |
| CLK (SCK) | 6 | D4 |
| CS | 10 | D10 |
| DC | 5 | D3 |
| RST | 4 | D2 |
| BUSY | 20 | D7 |

- V2.1 보드에서는 RST가 모듈 전원 스위치이기도 해서, 딥슬립 중 RST와 나머지 패널 선을 모두 LOW로 유지.
  [배터리 누설전류 Trouble shooting](deep-sleep-drain.md) 참고
- C3 기본 SPI 핀(SCK=4, MISO=5)이 RST(4), DC(5)와 겹침. `display.cpp`에서 `SPI.begin(6, -1, 7, 10)`으로
  재지정한 뒤 패널 핀을 다시 출력으로 설정하고 수동 리셋
- RST와 BUSY 모두 연결 유지. GxEPD2는 둘 다 `-1`을 받으면 소프트웨어 리셋과 고정 대기로 대체하지만,
  시도 결과 화면이 제대로 나오지 않음. 원인은 미확인

## 배터리 전압
화면 표시와 서버 보고에 사용.

| | GPIO | XIAO 라벨 |
|---|---|---|
| 분압 탭 | 3 | D1 |

`BAT+ ──[1M]──┬──[1M]── GND`, 가운데 탭은 D1, 탭과 GND 사이에 `100nF`. Wi-Fi가 켜지면 ADC2 사용
불가라 탭은 ADC1(GPIO0~4)에 연결. GPIO2는 스트래핑 핀이라 분압을 물리면 부팅 시 LOW로 읽힘. 그래서
BUSY를 GPIO3에서 다른 핀으로 이동.

## 실내 온습도 (SHT40)
없어도 동작.

| SHT40 | GPIO | XIAO 라벨 |
|---|---|---|
| VIN | — | 3V3 |
| GND | — | GND |
| SDA | 8 | D8 |
| SCL | 2 | D0 |

XIAO에 SDA, SCL로 표시된 핀은 e-Paper SPI가 쓰고 있어서 I2C 버스를 다른 핀으로 이동. 남는 핀 중
GPIO21이 먼저 보이지만 UART0 TX라서 리셋마다 ROM 부트로더가 구동. GPIO9도 비어 있지만 더 나쁜 선택:
부트 모드 선택 핀이라, SDA가 LOW로 멈춘 상태에서 리셋되면 앱 대신 다운로드 모드로 부팅하고 다시
깨어나지 않음.
