[English](jtag-debug.md) | [한국어](jtag-debug.ko.md)

# ESP32-C3 JTAG 디버깅

C3의 **내장 USB Serial/JTAG**으로 스텝 디버깅이 됨 — 외부 프로브 불필요. 단 OpenOCD 설정 3개와
빌드 플래그 1개가 필요하고, 셋 다 자기가 고치는 에러 메시지만 봐선 유추되지 않음.

설정은 `platformio.ini`의 `[dbg]` 섹션에 있고, `[env] extends = dbg`로 전 env가 상속함.

---

## 1. 설정

```ini
[dbg]
debug_tool = esp-builtin                       ; C3 내장 USB-JTAG
debug_init_break = tbreak setup                ; setup()에서 멈춤
debug_build_flags = -Og -g2 -DDEBUG_NO_SLEEP   ; 심볼 + 딥슬립 비활성 (§3)
debug_server =                                 ; openocd 경로와 gdb_port는 생략
    ...openocd
    -f interface/esp_usb_jtag.cfg
    -c set ESP_RTOS none                       ; (1)
    -f target/esp32c3.cfg
    -c adapter speed 5000
    -c gdb_memory_map disable                  ; (2)
    -c gdb_breakpoint_override hard            ; (3)
```

`tbreak`은 **임시** 브레이크포인트라 처음 한 번 걸리면 사라짐 — 리셋 이후에 문제가 됨 (§4).

---

## 2. 각 설정이 필요한 이유

**셋 다 필수이고, 그중 로그로 티를 내는 건 둘뿐.**

| 설정 | 없으면 깨지는 것 | 나오는 에러 |
|---|---|---|
| `set ESP_RTOS none` | OpenOCD가 **RTOS 초기화 전(ROM 단계)**에 FreeRTOS 태스크를 읽으려다 쓰레기값을 봄. 그 뒤 GDB가 스레드 상태를 놓쳐 세션이 "실행 중"에 갇히고 툴바 버튼도 먹통이 됨. | `Error: FreeRTOS maximum used priority is unreasonably big, not proceeding: 202`, 이어서 `continue` 시 *"Cannot execute this command while the selected thread is running"* |
| `gdb_memory_map disable` | 앱 실행 중에 OpenOCD가 접속 시점에 플래시 probe를 시도하다 실패 → 브레이크포인트를 걸기도 전에 GDB 접속 자체가 거부됨. | `Error: attempted 'gdb' connection rejected` (전문은 아래) |
| `gdb_breakpoint_override hard` | 메모리 맵이 없으면 GDB가 플래시와 RAM을 구분 못 해 소프트웨어 브레이크포인트를 고르는데, 플래시엔 심을 수 없음. | **없음** — 브레이크포인트는 등록되고 조용히 안 걸림 |

접속 거부 에러 전문:

```
Memory protection is enabled. Reset target to disable it...
Error: Failed to get flash maps (4294967295)!
Error: Failed to probe flash, size 0 KB
Error: auto_probe failed
Error: Connect failed. Consider setting up a gdb-attach event ... or use 'gdb_memory_map disable'
Error: attempted 'gdb' connection rejected
```

- **(2)와 (3)은 한 쌍**: OpenOCD가 에러 메시지에서 직접 해법을 알려주고, 메모리 맵을 끄는 것이
  곧 하드웨어 브레이크포인트를 강제해야 하는 이유가 됨. C3는 **하드웨어 트리거 8개** → 동시에
  8개까지.
- ESP32 Arduino는 **실제로 FreeRTOS 위에서** 돌아감(`setup()`/`loop()`도 태스크 안). 즉 (1)은
  태스크 인식 디버깅을 포기하고 동작하는 세션을 얻는 거래.

---

## 3. `DEBUG_NO_SLEEP`: 딥슬립 vs 디버거

**딥슬립에 들어가면 세션이 끝남** — 칩 전원이 내려가 JTAG이 끊김. 그래서 디버그 빌드에선 건너뜀:

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

## 4. 세션이 조용히 안 멈추는 이유

**브레이크포인트가 안 걸리는 원인은 두 가지이고, 둘 다 아무 메시지가 없음.** 앱이 안 돌고 있거나,
리셋이 디버거를 우회했거나.

![조용히 실패하는 두 경우. ① 앱이 실제로 돌고 있는가? 정상 부팅은 boot:0x8 (SPI_FAST_FLASH_BOOT)로 앱이 돌고 브레이크포인트가 걸림. 다운로드 모드는 boot:0x0 (USB_BOOT) + wait usb download로 앱이 시작조차 안 하고 CPU가 ROM에 머물러 아무것도 걸릴 수 없음. ② 리셋이 디버거를 거쳤는가? 디버거 리셋(Restart, monitor reset halt, PIO Debug without uploading)은 GDB가 하드웨어 브레이크포인트를 다시 심어 정상 동작. 리셋 버튼은 GDB 모르게 칩을 리셋해 트리거 레지스터가 지워지고 다시 심는 주체가 없어 이후로 영영 안 멈춤.](jtag-fail-modes.png)

### 다운로드 모드에서는 브레이크포인트가 걸리지 않음

**디버깅은 앱이 실행 중이어야 가능함.** USB 다운로드 모드면 앱이 시작조차 안 해서, 멈춰 세워도
심볼 없는 ROM에 있음:

```
Program received signal SIGINT, Interrupt.
0x400462dc in ?? ()
```

**BOOT+RESET은 플래싱용이지 디버깅용이 아님.** 빠져나오려면 BOOT를 누르지 말고 **USB를 뽑았다 다시
꽂을 것** — 소프트웨어 리셋만으론 다운로드 모드에서 안 빠져나옴.

### RESET 버튼은 하드웨어 브레이크포인트를 지움

**물리 RESET을 누르면 CPU의 하드웨어 브레이크포인트 트리거 레지스터가 지워지고, 그 사실이 디버거에
전달되지 않음** — 다시 심는 주체가 없으니 이후로 영영 안 멈춤. 다시 심어주는 건 디버거를 거친
리셋뿐:

| 리셋 방식 | 트리거 재설치 |
|---|---|
| **Restart** 버튼 | 됨 |
| Debug Console에서 `monitor reset halt` 후 `c` | 됨 |
| **PIO Debug (without uploading)** | 됨 — 코드가 그대로면 재플래시도 건너뜀 |
| 보드의 **RESET** 버튼 | **안 됨** — GDB 모르게 지워짐 |

에디터 브레이크포인트(빨간 점)는 GDB가 들고 있는 목록이라 리셋 후에도 남음. `debug_init_break =
tbreak setup`은 남지 않음 — 임시 브레이크포인트라 처음 걸릴 때 소비됨.

---

## 워크플로우

1. 보드가 자는 중(딥슬립 펌웨어)이면 플래싱을 위해 깨우기: **BOOT** 누른 채 **RESET** — 또는 USB
   재연결.
2. **F5**(Run and Debug). PlatformIO가 디버그 플래그로 빌드 → JTAG으로 플래시 → `setup()`에서 멈춤.
3. **F10**으로 스텝. **F11**은 정말 들어갈 때만.
4. 디버그 빌드는 안 자므로 이후 실행엔 BOOT+RESET 불필요. 코드가 그대로일 때 재플래시 없이 다시
   붙으려면 **PIO Debug (without uploading)**으로 시작할 것.

**네트워크 호출은 F10으로 넘길 것.** `connectWiFi()`/`http.GET()` 안에서 멈추면 CPU가 정지한 동안에도
TCP/TLS 타이머는 흘러서 연결이 죽고 동작이 달라짐.

툴바 버튼이 가끔 먹통이 되는데, Debug Console은 동작함:

| 명령 | |
|---|---|
| `c` | continue |
| `n` | next (스텝 오버) |
| `s` | step into |
| `bt` | 백트레이스 |
| `info breakpoints` | 현재 심긴 목록 |
| `monitor reset halt` | 디버거를 거친 리셋, 이후 `c` (§4) |

`platformio.ini`를 바꾸면 **Developer: Reload Window** 후에 반영됨.
