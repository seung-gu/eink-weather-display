[English](jtag-debug.md) | [한국어](jtag-debug.ko.md)

# JTAG debugging on the ESP32-C3

Step debugging works over the C3's **built-in USB Serial/JTAG** — no external probe. It takes three
OpenOCD overrides plus one build flag, none of which follow from the error messages they fix.

Config lives in the `[dbg]` section of `platformio.ini`, inherited by every env via
`[env] extends = dbg`.

---

## 1. The config

```ini
[dbg]
debug_tool = esp-builtin                       ; C3's internal USB-JTAG
debug_init_break = tbreak setup                ; stop at setup()
debug_build_flags = -Og -g2 -DDEBUG_NO_SLEEP   ; symbols + no deep sleep (§3)
debug_server =                                 ; openocd path and gdb_port elided
    ...openocd
    -f interface/esp_usb_jtag.cfg
    -c set ESP_RTOS none                       ; (1)
    -f target/esp32c3.cfg
    -c adapter speed 5000
    -c gdb_memory_map disable                  ; (2)
    -c gdb_breakpoint_override hard            ; (3)
```

`tbreak` is a **temporary** breakpoint, consumed on its first hit — which matters after a reset
(§4).

---

## 2. Why each override

**All three are mandatory, and only two announce themselves in the log.**

| Override | What breaks without it | The error it produces |
|---|---|---|
| `set ESP_RTOS none` | OpenOCD enumerates FreeRTOS tasks while the chip is still in ROM, before the RTOS has initialised, and reads garbage. GDB then loses thread state, the session sticks at "running" and the toolbar buttons go dead. | `Error: FreeRTOS maximum used priority is unreasonably big, not proceeding: 202`, then on `continue`: *"Cannot execute this command while the selected thread is running"* |
| `gdb_memory_map disable` | OpenOCD probes flash on attach and fails while the app is running, so GDB is refused at connect time — before any breakpoint can be set. | `Error: attempted 'gdb' connection rejected` (full text below) |
| `gdb_breakpoint_override hard` | With no memory map GDB cannot tell flash from RAM and picks software breakpoints, which cannot be planted in flash. | **none** — the breakpoint is accepted and silently never hits |

The connect-time refusal in full:

```
Memory protection is enabled. Reset target to disable it...
Error: Failed to get flash maps (4294967295)!
Error: Failed to probe flash, size 0 KB
Error: auto_probe failed
Error: Connect failed. Consider setting up a gdb-attach event ... or use 'gdb_memory_map disable'
Error: attempted 'gdb' connection rejected
```

- **(2) and (3) are one pair**: OpenOCD's own message names the fix, and disabling the memory map
  is exactly what makes forced hardware breakpoints necessary. The C3 has **8 hardware triggers**,
  so 8 breakpoints at a time.
- Arduino on ESP32 **does** run on FreeRTOS (`setup()`/`loop()` live in a task), so (1) trades
  task-aware debugging for a session that works.

---

## 3. `DEBUG_NO_SLEEP`: deep sleep vs the debugger

**Deep sleep ends the session** — the chip powers down and JTAG drops. The debug build skips it:

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

## 4. Why a session silently fails to stop

**A breakpoint that never hits has two causes, and neither is reported.** Either the app is not
running, or a reset went around the debugger.

![Two silent failure modes. ① Is the app actually running? Normal boot, boot:0x8 (SPI_FAST_FLASH_BOOT), the app runs and breakpoints hit; download mode, boot:0x0 (USB_BOOT) plus wait usb download, the app never starts and the CPU sits in ROM, so nothing can ever hit. ② Did the reset go through the debugger? A debugger reset — Restart, monitor reset halt, or PIO Debug without uploading — makes GDB re-plant the hardware breakpoints and they hit; the reset button resets the chip behind GDB's back, the trigger registers are cleared, nothing re-plants them, and execution never stops again.](jtag-fail-modes.png)

### Download mode blocks every breakpoint

**Debugging requires the app to be running.** In USB download mode it never starts, so a halt lands
in ROM with no symbols:

```
Program received signal SIGINT, Interrupt.
0x400462dc in ?? ()
```

**BOOT+RESET is for *flashing*, not for debugging.** The way out is to **unplug and replug USB**
without holding BOOT — a software reset alone does not leave download mode.

### The RESET button clears hardware breakpoints

**A physical RESET press clears the CPU's hardware breakpoint trigger registers, and does so
without the debugger's knowledge** — nothing re-plants them, so execution never stops again. Only a
debugger-mediated reset re-plants them:

| Reset | Re-plants the triggers |
|---|---|
| **Restart** button | yes |
| `monitor reset halt`, then `c` in the Debug Console | yes |
| **PIO Debug (without uploading)** | yes — and skips reflashing when the code has not changed |
| **RESET** button on the board | **no** — cleared behind GDB's back |

Editor breakpoints (the red dots) are GDB's own list and survive such a reset. `debug_init_break =
tbreak setup` does not: a temporary breakpoint is consumed on its first hit.

---

## Workflow

1. Board asleep (deep-sleep firmware)? Wake it for flashing: hold **BOOT**, tap **RESET** — or
   replug USB.
2. Press **F5** (Run and Debug). PlatformIO builds with debug flags, flashes over JTAG, and breaks
   at `setup()`.
3. Step with **F10**; **F11** only where you mean it.
4. Later runs need no BOOT+RESET — the debug build stays awake. To re-enter without reflashing
   unchanged code, launch **PIO Debug (without uploading)**.

**Step *over* (`F10`) network calls.** Halting inside `connectWiFi()`/`http.GET()` lets TCP/TLS
timers run while the CPU is stopped, so the connection dies and behaviour diverges.

Toolbar buttons occasionally stop responding; the Debug Console still works:

| Command | |
|---|---|
| `c` | continue |
| `n` | next (step over) |
| `s` | step into |
| `bt` | backtrace |
| `info breakpoints` | list what is planted |
| `monitor reset halt` | debugger-mediated reset, then `c` (§4) |

`platformio.ini` changes need **Developer: Reload Window** before they take effect.
