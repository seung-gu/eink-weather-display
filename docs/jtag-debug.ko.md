[English](jtag-debug.md) | [한국어](jtag-debug.ko.md)

# ESP32-C3 JTAG 디버깅

C3의 **내장 USB Serial/JTAG**으로 스텝 디버깅이 됨 — 외부 프로브 불필요. 다만 OpenOCD 설정
3개 + 펌웨어 변경 1개가 필요했고, 에러 메시지만 봐선 알 수 없어서 여기 기록함.

설정은 `platformio.ini`의 `[dbg]` 섹션에 있고, `[env] extends = dbg`로 전 env가 상속함.

---

## 설정

```ini
[dbg]
debug_tool = esp-builtin                       ; C3 내장 USB-JTAG
debug_init_break = tbreak setup                ; setup()에서 멈춤
debug_build_flags = -Og -g2 -DDEBUG_NO_SLEEP   ; 심볼 + 딥슬립 비활성 (아래 참고)
debug_server =
    ...openocd
    -f interface/esp_usb_jtag.cfg
    -c set ESP_RTOS none                       ; (1)
    -f target/esp32c3.cfg
    -c adapter speed 5000
    -c gdb_memory_map disable                  ; (2)
    -c gdb_breakpoint_override hard            ; (3)
```

---

## 각 설정이 필요한 이유

### (1) `set ESP_RTOS none`

없으면 OpenOCD가 **RTOS 초기화 전(ROM 단계)**에 FreeRTOS 태스크를 읽으려다 쓰레기값을 봄:

```
Error: FreeRTOS maximum used priority is unreasonably big, not proceeding: 202
```

그 뒤 GDB가 스레드 상태를 놓쳐 세션이 "실행 중"에 갇힘 — `continue`가
*"Cannot execute this command while the selected thread is running"*으로 실패하고,
툴바 버튼도 먹통이 됨.

참고: ESP32 Arduino는 **실제로 FreeRTOS 위에서** 돌아감(`setup()`/`loop()`도 태스크 안). 그래서
이 설정은 태스크 인식 디버깅을 포기하는 것 — 대신 세션이 정상 동작함.

### (2) `gdb_memory_map disable`

없으면 앱이 실행 중일 때 GDB 접속 자체가 거부됨:

```
Memory protection is enabled. Reset target to disable it...
Error: Failed to get flash maps (4294967295)!
Error: Failed to probe flash, size 0 KB
Error: auto_probe failed
Error: Connect failed. Consider setting up a gdb-attach event ... or use 'gdb_memory_map disable'
Error: attempted 'gdb' connection rejected
```

OpenOCD가 에러 메시지에서 직접 해법을 알려줌. 이걸 끄면 접속 시 플래시 probe를 안 하고,
그래서 (3)이 필요해짐.

### (3) `gdb_breakpoint_override hard`

메모리 맵이 없으면 GDB가 플래시와 RAM을 구분 못 해 **소프트웨어 브레이크포인트**를 고르는데,
플래시엔 심을 수 없음. 하드웨어 브레이크포인트를 강제하면 정상 동작 (C3는 트리거 8개 → 동시에
8개까지).

---

## `DEBUG_NO_SLEEP`: 딥슬립 vs 디버거

딥슬립에 들어가면 칩 전원이 내려가 JTAG이 끊기고 디버그 세션이 죽음. 그래서 디버그 빌드에선
건너뜀:

```cpp
#ifndef DEBUG_NO_SLEEP
  esp_sleep_enable_timer_wakeup(...);
  esp_deep_sleep_start();
#else
  Serial.println("[debug] staying awake (no deep sleep)");
#endif
```

`debug_build_flags`가 `DEBUG_NO_SLEEP`을 **디버그 빌드에만** 정의하므로, 일반
`pio run -t upload`는 그대로 딥슬립함. 코드 주석을 넣었다 뺐다 할 필요 없음.

---

## 가장 오래 잡아먹은 함정: 다운로드 모드

디버깅은 **앱이 실행 중**이어야 가능함. 보드가 USB 다운로드 모드면 브레이크포인트가 절대 안 걸림
— 앱이 시작조차 안 하고 ROM에서 멈춰 있음:

```
Program received signal SIGINT, Interrupt.
0x400462dc in ?? ()
```

시리얼의 부팅 줄로 확인:

| 부팅 로그 | 의미 |
|---|---|
| `boot:0x8 (SPI_FAST_FLASH_BOOT)` | 앱 실행 중 — 디버깅 가능 |
| `boot:0x0 (USB_BOOT)` + `wait usb download` | 다운로드 모드 — **아무것도 안 걸림** |

BOOT+RESET은 **플래싱용**이지 디버깅용이 아님. 다운로드 모드에 갇히면 **USB를 뽑았다 다시 꽂을 것**
(BOOT는 누르지 말고) — 소프트웨어 리셋만으론 안 빠져나옴.

---

## 워크플로우

1. 보드가 자는 중(딥슬립 펌웨어)이면 플래싱을 위해 깨우기: **BOOT** 누른 채 **RESET** —
   또는 USB 재연결.
2. **F5**(Run and Debug). PlatformIO가 디버그 플래그로 빌드 → JTAG으로 플래시 → `setup()`에서 멈춤.
3. **F10**으로 스텝. **F11**은 정말 들어갈 때만.
4. 디버그 빌드는 안 자므로, 이후 F5는 BOOT+RESET 없이 바로 됨.

참고:
- 네트워크 호출은 **F10으로 넘길 것**. `connectWiFi()`/`http.GET()` 안에서 멈추면 CPU가 정지한
  동안에도 TCP/TLS 타이머는 흘러서 연결이 죽고 동작이 달라짐.
- 툴바 버튼이 가끔 먹통이 되는데, Debug Console은 동작함 — `c`(continue), `n`(next), `s`(step),
  `bt`, `info breakpoints`.
- `platformio.ini`를 바꾸면 **Developer: Reload Window** 후에 반영됨.
