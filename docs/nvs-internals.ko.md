[English](nvs-internals.md) | [한국어](nvs-internals.ko.md)

# NVS가 플래시에 실제로 쓰는 것

`prefs.putString("last", w)` 코드 세 줄이 칩 안에서 어떤 모습인지 — 문서가 아니라 **실제 기기를
덤프해서** 확인한 내용.

날씨를 NVS에 저장하는 *이유*는 [저장과 화면 갱신](persistence-and-refresh.ko.md) 참고.

---

## 구조

![NVS 구조: 플래시 파티션 테이블, 4KB 페이지 하나, 실제 데이터 엔트리](nvs-layout.png)

| 계층 | 내용 |
|---|---|
| **파티션** | 플래시 `0x9000`의 `nvs`, 20KB — 파티션 테이블에 예약된 영역 |
| **페이지** | 4KB씩 5개: 헤더(32B) + 엔트리 상태 비트맵(32B) + 엔트리 126개 |
| **엔트리** | 32B. 키를 담고, 값이 작으면 값까지 함께 |

---

## 덤프 재현

시리얼 포트가 비어 있어야 함 (PlatformIO 모니터·디버그 세션 먼저 종료):

```bash
# 1. nvs 파티션 20KB 덤프
python $(find ~/.platformio/packages/tool-esptoolpy -name esptool.py) \
  --chip esp32c3 --port /dev/cu.usbmodem11401 \
  read_flash 0x9000 0x5000 nvs.bin

# 2. 문자열 위치 찾기 (오프셋은 파티션 기준, 플래시 주소는 +0x9000)
grep -abo "weather" nvs.bin     # 네임스페이스 -> 12360 (0x3048) -> flash 0xC048
grep -abo "last"    nvs.bin     # 키          -> 6군데
xxd -s 0x3040 -l 128 nvs.bin    # 원본 바이트 보기
```

---

## 32바이트 엔트리

```
byte  0      NsIndex     네임스페이스 번호 (이름이 아님!)
byte  1      Type        0x21 = 문자열, 0x01 = uint8, …
byte  2      Span        이 항목이 32B 칸을 몇 개 쓰는지
byte  3      ChunkIndex
bytes 4–7    CRC32
bytes 8–23   Key         16B, 널 패딩
bytes 24–31  Data        작은 값은 여기 직접, 큰 값은 길이 + CRC만
```

## 네임스페이스 → 번호

네임스페이스 문자열은 `NsIndex = 0`인 특별 엔트리에 **딱 한 번** 저장됨:

```
0xC040:  00 01 01 ff  7696324c  "weather" 00…  07
         │  │  │       │         │              └─ 값: 7
         │  │  │       │         └─ Key = "weather"
         │  │  │       └─ CRC32
         │  │  └─ Span = 1
         │  └─ Type = 0x01 (uint8)
         └─ NsIndex = 0  → 네임스페이스 이름표 전용
```

이후 엔트리들은 이 **1바이트 번호**로만 참조함. 덤프에서 `"weather"`는 한 번,
`"last"`는 여러 번 보이는 이유가 이것.

## 우리 키와 값

```
0xC060:  07 21 03 ff  ea65d479  "last" 00…  3a 00 ff ff  0d926a00
         │  │  │                 │           │            └─ 데이터 CRC32
         │  │  │                 │           └─ 길이 = 0x003A = 58바이트
         │  │  │                 └─ Key = "last"
         │  │  └─ Span = 3  (헤더 1칸 + 데이터 2칸)
         │  └─ Type = 0x21 (문자열)
         └─ NsIndex = 7  → "weather"

0xC080:  4d c3bc 6e 63 68 6e 65 72 20 46 72 65 69 68 65 69 74 0a
          M  ü    n  c  h  n  e  r     F  r  e  i  h  e  i  t  \n
0xC090:  31 36 c2b0 43 0a  ed9d90 eba6bc 0a  …
          1  6  °    C  \n   흐     림    \n
```

`ü` = `0xC3 0xBC`, `°` = `0xC2 0xB0` — UTF-8 2바이트로, **디버거가 RAM에서 보여준 바이트와 동일**.
플래시로 갈 때 재인코딩 같은 건 없음.

---

## 쓰기 비용이 싼 이유

덤프에서 `"last"`가 **6번** 나옴 — `0xC068`, `0xC0E8`, `0xC188`, `0xC228`, `0xC2E8`, `0xC388`.
갱신할 때마다 덮어쓴 게 아니라 **새 엔트리를 추가**한 것:

| | |
|---|---|
| 값 갱신 | 새 엔트리 기록 + 옛 엔트리는 페이지 비트맵에 *삭제됨* 표시만 |
| 섹터 지우기 | 페이지에 빈 엔트리가 없을 때만 |
| 같은 값 재저장 | NVS가 아예 건너뜀 |

NOR 플래시는 제자리에서 비트를 되돌릴 수 없어서, 덮어쓰려면 4KB 페이지를 통째로 지워야 함.
추가 방식이 그걸 피하고, 그래서 하루 ~144회 쓰기가 수십 년 버팀 —
[수명 계산](persistence-and-refresh.ko.md#플래시-수명) 참고.

---

## 우리 것이 아닌 것

```
0xC0C0:  06 04 01 ff  a16e5ce3  "WIFI_STA_DEF" 00…  c0 a8 01 b3
                                                      192 168  1  179
```

WiFi 스택도 자기 NVS 엔트리를 씀. 마지막 IP 주소가 캐시돼 있고, 알던 AP에 재접속이
~140ms에 끝나는 이유 중 하나.

---

## 참고

- NVS 데이터 영역은 **메모리 매핑이 안 됨**. 코드·상수는 GDB에서 `0x42000000`(IROM) /
  `0x3C000000`(DROM)으로 읽히지만, NVS는 플래시 드라이버로만 접근 → GDB에서 `x/ 0x9000`은 무의미.
- API가 주소를 **일부러 숨김**. NVS가 엔트리 위치를 옮기기 때문에 `getAddress()` 같은 게 없음.
  위 덤프가 물리적 위치를 보는 유일한 방법.
- 일반 펌웨어 업로드는 이 파티션을 안 건드림. `pio run -t erase`는 지움.
