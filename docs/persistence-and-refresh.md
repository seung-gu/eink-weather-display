[English](persistence-and-refresh.md) | [한국어](persistence-and-refresh.ko.md)

# Persistence & refresh notes

Why the last weather lives in **NVS (flash)** instead of RTC memory, and why the status line
is drawn with a **full refresh** instead of a partial one.

Context: every wake is a full reset (deep sleep). When Wi-Fi or the fetch fails we still want
the screen to show the last known weather plus the *current* status (`offline`, signal gauge).

---

## 1. Why not a partial refresh of just the bottom strip?

The plan was: leave the weather area untouched, `setPartialWindow()` the bottom ~22 px, redraw
only the status line. What we got instead:

![Whole panel filled with random pixel noise after a partial refresh](partial-refresh-noise.jpg)

The **entire panel** turned into random noise — not just the strip we asked to update.

### What the driver actually does

Two separate mechanisms broke it, both in the installed GxEPD2:

**a) `init(..., initial=true)` ignores the partial window.**

```cpp
// GxEPD2_154_D67.cpp:274 — refresh(x, y, w, h)
if (_initial_refresh) return refresh(false);   // initial update needs be full update
```

`init(bitrate)` (and `init(bitrate, true)`) sets `_initial_refresh = true`, so the first
`refresh(x,y,w,h)` silently becomes a **full-screen** `_Update_Full()`. The partial window is
discarded. Result: the weather area was cleared to white.

**b) `init(..., initial=false)` refreshes from garbage controller RAM.**

Partial update is *differential*: `_Update_Part()` drives the panel from the controller's own
RAM (0x24 "current" / 0x26 "previous"). That RAM is **not** the ESP32's memory — it lives in the
SSD1681 on the display module, and we destroy it on every wake:

- `display.hibernate()` puts the controller into deep sleep (`0x10`/`0x01`) → RAM lost
- `displayBegin()` issues a manual RST pulse (needed for the C3 SPI pin fix) → controller reset
- `init(..., false)` deliberately skips the initial clear, so nothing refills that RAM

Writing only the strip therefore leaves everything outside it undefined, and the update drives
the whole panel from that undefined RAM — the photo above.

The library documents this exact failure mode:

```
// GxEPD2_BW.h:320
// NOTE: garbage will result on fast partial update displays,
//       if initial full update is omitted after power loss
```

### Conclusion

A correct partial refresh would require the **entire** controller RAM to hold the current image
first — i.e. redrawing the full screen anyway, then refreshing only the strip. That buys a
shorter refresh (0.5 s vs 2.6 s) at the cost of a fragile dependency on controller state that a
deep-sleep device resets on every wake. Not worth it here: **always full refresh**.

---

## 2. Why NVS (flash), not RTC memory?

To redraw the previous weather after a failed fetch we need the last good response to survive
the wake. Two options:

| | `RTC_DATA_ATTR` (RTC SRAM) | **NVS (flash)** |
|---|---|---|
| Deep sleep wake | survives | survives |
| Reset button / `ESP.restart()` | **lost** | survives |
| Power loss, battery swap | **lost** | survives |
| Firmware re-upload | **lost** | survives (NVS partition is untouched) |
| Access | direct variable, no copying | `Preferences` get/put |

RTC memory is only retained across *deep sleep*. During development — reset button, re-flash —
it is wiped every time, which is exactly when the screen went blank. NVS survives all of it.

### Cost

Current draw is effectively identical; the awake Wi-Fi window dominates everything:

| | draw |
|---|---|
| Wi-Fi connect + HTTPS | ~80–120 mA for 1–4 s ← dominates |
| e-Paper full refresh | ~10 mA for 2.6 s |
| **NVS write** | ~15 mA for ~10 ms (≈ 0.04 mAs) |
| Deep sleep (either option) | ~5 µA — the RTC domain is already powered for the wake timer |

### Flash wear

NOR flash endures ~100,000 erase cycles per sector, and NVS appends entries into a page,
erasing only when the page fills — plus it skips the write entirely when the value is unchanged.

At one write per 10 min (~52,500/year), with a ~100-byte value and the default ~20 KB NVS
partition, that works out to a few hundred sector erases per year: **decades of headroom**.
This only becomes a concern at second-level intervals.

---

## 3. Resulting flow

```cpp
WifiResult wifi = connectWiFi();          // 1 try + 1 retry, no restart loop

String fetched;
if (wifi.ok) fetched = httpGet(WEATHER_URL);   // 1 try + 1 retry (server cold start)

// NVS is the single source of truth: store what's fresh, then draw what's stored.
prefs.begin("weather", false);
if (fetched.length()) prefs.putString("last", fetched);
String w = prefs.getString("last", "");
prefs.end();

displayBegin();                                     // always redraw, full refresh
displayWeather(w, wifi.ok ? wifi.ms : 0,            // 0 ms  -> "offline"
                  wifi.ok ? wifi.rssi : 0);         // 0 dBm -> empty gauge
```

| Situation | Screen |
|---|---|
| Wi-Fi ok, fetch ok | new weather + connect time + signal gauge |
| Wi-Fi ok, fetch failed | last weather (NVS) + connect time + gauge |
| Wi-Fi failed | last weather (NVS) + `offline` + empty gauge |
| No stored data yet | empty frame + `offline` |

The status line always reflects the *current* wake, so a stale screen is still distinguishable
from a fresh one.
