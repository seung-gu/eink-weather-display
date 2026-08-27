[English](jtag-debug.md) | [한국어](jtag-debug.ko.md)

# JTAG debugging on the ESP32-C3

Step debugging works over the C3's **built-in USB Serial/JTAG** — no external probe. Getting
there needed three OpenOCD overrides plus one firmware change, all recorded here because none
of them are obvious from the error messages.

Config lives in the `[dbg]` section of `platformio.ini`, inherited by every env via
`[env] extends = dbg`.

---

## The setup

```ini
[dbg]
debug_tool = esp-builtin                       ; C3's internal USB-JTAG
debug_init_break = tbreak setup                ; stop at setup()
debug_build_flags = -Og -g2 -DDEBUG_NO_SLEEP   ; symbols + no deep sleep (see below)
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

## Why each override

### (1) `set ESP_RTOS none`

Without it, OpenOCD tries to enumerate FreeRTOS tasks while the chip is still in ROM, before the
RTOS has initialised, and reads garbage:

```
Error: FreeRTOS maximum used priority is unreasonably big, not proceeding: 202
```

GDB then loses track of thread state and the session gets stuck "running" — `continue` fails
with *"Cannot execute this command while the selected thread is running"*, and the toolbar
buttons go dead.

Note that Arduino on ESP32 **does** run on FreeRTOS (`setup()`/`loop()` live in a task), so this
disables task-aware debugging — a fair trade for a session that actually works.

### (2) `gdb_memory_map disable`

Must be set, or GDB is refused at connect time while the app is running:

```
Memory protection is enabled. Reset target to disable it...
Error: Failed to get flash maps (4294967295)!
Error: Failed to probe flash, size 0 KB
Error: auto_probe failed
Error: Connect failed. Consider setting up a gdb-attach event ... or use 'gdb_memory_map disable'
Error: attempted 'gdb' connection rejected
```

OpenOCD's own message names the fix. With the memory map disabled it stops probing flash on
attach, which is also why (3) is needed.

### (3) `gdb_breakpoint_override hard`

With no memory map, GDB can't tell flash from RAM and would pick software breakpoints, which
can't be planted in flash. Forcing hardware breakpoints makes them land (the C3 has 8 triggers,
so that many at a time).

---

## `DEBUG_NO_SLEEP`: deep sleep vs the debugger

Deep sleep kills the debug session — the chip powers down and JTAG drops. So the debug build
skips it:

```cpp
#ifndef DEBUG_NO_SLEEP
  esp_sleep_enable_timer_wakeup(...);
  esp_deep_sleep_start();
#else
  Serial.println("[debug] staying awake (no deep sleep)");
#endif
```

`debug_build_flags` defines `DEBUG_NO_SLEEP` **only** for debug builds, so a normal
`pio run -t upload` still deep-sleeps. No commenting code in and out.

---

## The part that cost the most time: download mode

Debugging requires the app to be **running**. If the board is in USB download mode, no
breakpoint will ever hit — the app never starts, and everything stalls in ROM:

```
Program received signal SIGINT, Interrupt.
0x400462dc in ?? ()
```

Check the boot line over serial:

| Boot log | Meaning |
|---|---|
| `boot:0x8 (SPI_FAST_FLASH_BOOT)` | app running — debuggable |
| `boot:0x0 (USB_BOOT)` + `wait usb download` | download mode — **nothing will hit** |

BOOT+RESET is for *flashing*, not for debugging. If the board is stuck in download mode,
**unplug and replug USB** (without holding BOOT) — a software reset alone won't leave it.

---

## Workflow

1. Board asleep (deep-sleep firmware)? Wake it for flashing: hold **BOOT**, tap **RESET** —
   or just replug USB.
2. Press **F5** (Run and Debug). PlatformIO builds with debug flags, flashes over JTAG, and
   breaks at `setup()`.
3. Step with **F10**; **F11** only where you mean it.
4. Since the debug build stays awake, later F5 runs need no BOOT+RESET dance.

Notes:
- Step **over** (`F10`) network calls. Halting inside `connectWiFi()`/`http.GET()` lets TCP/TLS
  timers run while the CPU is stopped, so the connection dies and behaviour diverges.
- Toolbar buttons occasionally stop responding; the Debug Console still works — `c` (continue),
  `n` (next), `s` (step), `bt`, `info breakpoints`.
- `platformio.ini` changes need **Developer: Reload Window** before they take effect.
