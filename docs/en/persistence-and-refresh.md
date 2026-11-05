[English](persistence-and-refresh.md) | [한국어](../ko/persistence-and-refresh.md)

# Persistence & refresh

A deep-sleep wake is a full reset: execution restarts at `setup()` with ordinary RAM gone, and the
panel controller comes up reset as well. Two decisions follow: the last good response is kept in
**NVS (flash)**, not RTC memory, and the screen is redrawn with a **partial refresh** over the last
frame, which each wake keeps in RTC memory for the next.

The requirement behind both: when Wi-Fi or the fetch fails, the screen still shows the last known
weather plus the status of the *current* wake (`offline`, signal gauge).

Refresh times and currents are this hardware's — an ESP32-C3 with a 1.54" SSD1681 panel — and are
marked where they appear. The GxEPD2 behaviour holds on any board.

Related: [Storing state in NVS](nvs-internals.md).

---

## 1. A partial refresh needs the last frame back in the panel's RAM

A partial update is *differential*: it drives only the pixels that differ from the previous image,
so the current screen must already sit in the panel controller's own RAM — on the display module,
not the ESP32's memory. Every wake leaves that RAM undefined, which breaks both `init()` modes.

![Three panels: a partial update draws from the panel controller's own RAM (0x24 current, 0x26 previous); every wake destroys that RAM through display.hibernate(), the manual RST pulse and init(…, false); so init(…, true) forces a full refresh and clears the weather area to white, while init(…, false) draws from undefined RAM and fills the whole panel with noise; so the kept frame is written back to 0x26](../images/epd-partial-fail.png)

The installed GxEPD2 states both modes in its source:

```cpp
// GxEPD2_154_D67.cpp:274 — refresh(x, y, w, h)
if (_initial_refresh) return refresh(false);   // initial update needs be full update
```

`init(bitrate)` — the same as `init(bitrate, true)` — sets `_initial_refresh = true`, so the first
`refresh(x, y, w, h)` silently becomes a full-screen `_Update_Full()` and the partial window is
discarded.

```
// GxEPD2_BW.h:320
// NOTE: garbage will result on fast partial update displays,
//       if initial full update is omitted after power loss
```

`init(…, false)` skips that initial full update, so `_Update_Part()` drives the whole panel from
RAM the wake left undefined.

Observed on this panel — `setPartialWindow()` over the bottom ~22 px after `init(…, false)`:

![The whole e-Paper panel filled with random pixel noise after a partial refresh of the bottom status strip](../images/partial-refresh-noise.jpg)

### The trade-off

| | Refresh time *(this panel)* | Precondition |
|---|---|---|
| Partial | 0.5 s | the whole current image already in controller RAM |
| **Full** | 2.6 s | none |

A device that resets the controller on every wake has to meet that precondition itself.

### Writing the last frame back

Each wake draws on a canvas in ESP32 memory, because GxEPD2 keeps its own buffer private, and keeps
a copy of the frame in RTC memory (5000 bytes). The next wake starts from that copy:

| Step | Call |
|---|---|
| Start without GxEPD2's forced first full refresh | `display.init(115200, false)` |
| The kept frame, which is what the screen shows, into 0x26 | `epd2.writeImageToPrevious()` |
| The new frame into 0x24 | `epd2.writeImage()` |
| Partial refresh of the whole screen | `epd2.refresh(0, 0, 200, 200)` |

A full refresh still runs when there is no frame to start from, and every 12 hours:

| Full refresh | Why |
|---|---|
| After a reset, a power cut or an update | RTC memory is emptied, so there is no kept frame |
| After a message or a QR | Only a weather screen is kept as a base |
| Every 12 hours (24 refreshes at 30 minutes) | Insurance. Testing showed no ghosting, with the city moved and every value and the icon changing |

Wakes with a partial refresh took 1.9–2.5 s in testing, against about 4.6 s with a full refresh.

---

## 2. The last response lives in NVS (flash), not RTC memory

Redrawing the previous weather after a failed fetch requires the last good response to outlive the
wake. RTC SRAM outlives deep sleep only; NVS outlives every reset this device sees.

| | `RTC_DATA_ATTR` (RTC SRAM) | **NVS (flash)** |
|---|---|---|
| Deep sleep wake | survives | survives |
| Reset button / `ESP.restart()` | **lost** | survives |
| Power loss, battery swap | **lost** | survives |
| Firmware re-upload | **lost** | survives (NVS partition is untouched) |
| Access | direct variable, no copying | `Preferences` get/put |

Reset presses and re-flashes are routine during development, and each one empties RTC memory,
leaving the screen with nothing to redraw.

### Energy cost

The NVS write disappears next to the awake Wi-Fi window:

| | draw *(this board)* |
|---|---|
| Wi-Fi connect + HTTPS | ~80–120 mA for 1–4 s ← dominates |
| e-Paper full refresh | ~10 mA for 2.6 s |
| **NVS write** | ~15 mA for ~10 ms (≈ 0.04 mAs) |
| Deep sleep (either option) | ~5 µA — the RTC domain is already powered for the wake timer |

### Flash wear

NVS appends entries into a page and erases only when the page fills, and it skips the write
entirely when the value is unchanged. At this wake interval the erase budget is nowhere near the
limit:

| | |
|---|---|
| NOR endurance | ~100,000 erase cycles per sector |
| Writes | 1 per 10 min ≈ 52,500/year |
| Value size | ~100 bytes |
| `nvs` partition | ~20 KB (default) |
| Sector erases | a few hundred per year |
| **Headroom** | **decades** |

Second-level wake intervals are where this turns into a concern. Slot-level accounting:
[Storing state in NVS](nvs-internals.md) §6.

---

## 3. The resulting flow

```cpp
WifiResult wifi = connectWiFi();          // 1 try + 1 retry, no restart loop

String fetched;
if (wifi.ok) fetched = httpGet(WEATHER_URL);   // 1 try + 1 retry (server cold start)

// NVS is the single source of truth: store what's fresh, then draw what's stored.
prefs.begin("weather", false);
if (fetched.length()) prefs.putString("last", fetched);
String w = prefs.getString("last", "");
prefs.end();

displayBegin();                                     // always redraw, partial or full (§1)
displayWeather(w, wifi.ok ? wifi.ms : 0,            // 0 ms  -> "offline"
                  wifi.ok ? wifi.rssi : 0);         // 0 dBm -> empty gauge
```

| Situation | Screen |
|---|---|
| Wi-Fi ok, fetch ok | new weather + connect time + signal gauge |
| Wi-Fi ok, fetch failed | last weather (NVS) + connect time + gauge |
| Wi-Fi failed | last weather (NVS) + `offline` + empty gauge |
| No stored data yet | empty frame + `offline` |

The status line always reflects the *current* wake, so a stale screen stays distinguishable from a
fresh one.
