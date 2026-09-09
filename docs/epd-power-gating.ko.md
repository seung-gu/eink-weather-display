[English](epd-power-gating.md) | [한국어](epd-power-gating.ko.md)

# e-Paper 모듈을 GPIO로 급전하기

`display.hibernate()`는 SSD1681 컨트롤러만 재운다. 모듈의 나머지는 그대로다 — Waveshare 캐리어
보드에는 레벨 시프터와 자체 전원 회로가 있고, 이것들은 딥슬립 내내 3V3에서 전류를 끌어간다.

모듈 VCC 선을 3V3 핀에서 뽑으면 측정되는 소비가 뚜렷하게 떨어진다. 이 문서는 그 전환을 코드로
자동화하는 방법과, 그 변경이 펌웨어에 들어가 있지 않은 이유를 남긴다.

---

## 1. 변경 내용

**배선**: 모듈 VCC를 보드의 3V3 핀에서 **D7 (GPIO20)**으로 옮긴다.

**`config.h`**

```c
#define EPD_PWR   20    // module VCC, switched (XIAO D7)
```

**`display.cpp` — 통신 전에 전원부터**

```cpp
void displayBegin() {
  pinMode(EPD_PWR, OUTPUT);
  digitalWrite(EPD_PWR, HIGH);
  delay(10);                  // let the rail settle before the reset pulse
  display.init(115200);
  ...
```

이 지연이 중요하다. 바로 뒤에 `display.init()`과 수동 RST 펄스가 이어지는데, 레일이 올라오기
전에 친 리셋은 컨트롤러를 어중간하게 초기화된 상태로 남긴다.

**`display.cpp` — 나갈 때 전원 차단**

```cpp
void displayPowerOff() {
  for (int p : {EPD_CS, EPD_DC, EPD_RST, EPD_SCK, EPD_MOSI, EPD_PWR}) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
  pinMode(EPD_BUSY, INPUT);
}
```

**`main.cpp` — 딥슬립 직전**

```cpp
#include "driver/gpio.h"
...
displayPowerOff();
gpio_deep_sleep_hold_en();
esp_deep_sleep_start();
```

---

## 2. 전원보다 신호선을 먼저 내리는 이유

전원 없는 모듈에 HIGH로 남은 GPIO는 입력 클램프 다이오드를 통해 죽은 전원 레일로 전류를
흘려보내고, 보드 일부를 계속 살려둔다. 펌웨어가 구동하는 모든 선을 먼저 0V로 내리고,
`EPD_PWR`을 마지막에 내린다.

**BUSY만 예외다.** 이 핀은 모듈의 출력이다. 출력으로 구동하면 한 배선에 드라이버가 둘 붙어서,
양쪽에 전원이 있는 동안 서로 단락된다. 입력으로 둔다.

---

## 3. D6이 아니라 D7인 이유

| 핀 | |
|---|---|
| D0 (GPIO2), D8 (GPIO8), D9 (GPIO9) | 스트래핑 핀 — 부팅 때 샘플링됨 |
| D6 (GPIO21) | UART0 TX. ROM 부트로더가 매 부팅마다 구동하고, UART 유휴 상태는 HIGH |
| **D7 (GPIO20)** | UART0 RX — 부팅 시 입력이고 ROM이 구동하지 않음 |

이 보드의 시리얼 출력은 네이티브 USB로 나가므로 UART0은 다른 용도로 비어 있다.

---

## 4. `gpio_deep_sleep_hold_en()`에는 대가가 있다

이 함수가 없으면 딥슬립에 들어가면서 모든 패드가 기본 상태로 돌아가고, `EPD_PWR`이 모듈을 꺼둔
상태를 유지하지 못한다. 이 함수를 쓰면 `displayPowerOff()`가 남긴 상태 그대로 잠긴다.

대가는 헤더에 적혀 있다 (`driver/gpio.h`):

> The state of each pad holds is its active configuration (**not pad's sleep configuration!**)

ESP-IDF는 원래 딥슬립 때 패드를 더 낮은 전력의 sleep 설정으로 옮긴다. 홀드는 입력 버퍼와 풀
저항을 액티브 모드 상태 그대로 유지시키므로, 떠 있는 핀이 있으면 딥슬립 내내 관통전류가 흐른다.
구동하지 않는 핀은 띄워두지 말고 풀을 걸어야 한다 — BUSY라면 `INPUT_PULLDOWN`.

---

## 5. 펌웨어에 들어가 있지 않은 이유

이득이 측정된 적이 없다. 소비를 배터리 전압으로 추적했는데, 충전기에서 뗀 리튬셀은 표면전하가
빠지면서 몇 시간 동안 저절로 내려간다 — 펌웨어가 무엇을 하든 기울기가 단조롭게 완만해진다.
이 방식으로 잰 변경은 전부 개선처럼 보인다. 측정 사이에 시간이 흘렀기 때문이다.

셀이 안정된 뒤에는 게이팅한 쪽(D7)과 안 한 쪽(3V3)이 같게 측정됐다.

---

## 6. 제대로 재는 방법

원리는 실재하므로 남은 질문은 그 값어치가 얼마냐뿐이다. 전압으로는 답이 안 나온다:

| | |
|---|---|
| **전류계 직렬, µA 레인지** | 한 번의 측정으로 끝난다. 수십 µA와 mA는 미묘한 차이가 아니다 |
| 며칠에 걸친 전압 | 셀을 충분히 쉬게 한 뒤, 비슷한 충전 상태에서 같은 길이의 구간끼리만 |

여기에 공을 들이기 전에 wake 예산과 비교해볼 만하다. 10분 주기에서 12초를 ~100mA로 깨어 있으면
하루 약 48mAh인데, 이상적인 딥슬립 바닥은 하루 약 1mAh다. 딥슬립 바닥이 mA 단위가 아닌 한
wake 쪽이 지배한다.
