# Changelog

What changed in each firmware release, newest first. Boards with auto update on install a release on their next wake.

- **major**: you have to do something, such as flash over USB again, register again, or change the board or the panel
- **minor**: the board does something new or behaves differently, and you have nothing to do
- **patch**: the same behavior, fixed or faster

The releases up to v1.1.0 were first tagged v1, v1.01, v1.03 and v1.04, and boards that installed them still report those names. v1.02 changed only the release workflow, so it has no entry.

## v1.4.1 · patch · 2026-11-12

- The radio is on one to three seconds less per report again. v1.4.0 made the secure connection to the server twice for each report instead of once.

## v1.4.0 · minor · 2026-11-12

- Each report also says how long each step of the previous wake took, from the readings through joining Wi-Fi, the secure connection and the server's answer to the screen refresh and an update download. The dashboard lists them under the awake time, so a longer wake can be traced to the step that grew.

## v1.3.1 · patch · 2026-11-11

- The report after a firmware download no longer repeats the radio time of the wake before it. It carries that wake's own, for the report alone; the download shows only in the awake time.

## v1.3.0 · minor · 2026-11-11

- When an update fails on all three tries, the board tells the server, which turns its auto update off and shows why on the dashboard. Turning auto update back on makes the board try that version again from the start.

## v1.2.2 · patch · 2026-11-11

- When an update does not take, the board tries again on its next scheduled wake instead of right away, up to three times per version instead of two back to back.

## v1.2.1 · patch · 2026-11-11

- The same firmware as v1.2.0, released again. The server sent the v1.1.1 file under the v1.2.0 name, so boards installed v1.1.1 twice and stopped trying v1.2.0. This release lets them install it.

## v1.2.0 · minor · 2026-11-11

- Each report also says how long the radio was on in the previous wake, from bringing Wi-Fi up to putting it down. The dashboard shows it next to the Wi-Fi time, so a longer wake can be traced to the network or to the screen refresh.

## v1.1.1 · patch · 2026-11-05

- The board reports its version under the new names (v1.1.1, not v1.04). Nothing else changed from v1.1.0.

## v1.1.0 · minor · 2026-11-05 (was v1.04)

- The screen refreshes partially, over the last frame kept in RTC memory, instead of fully on every wake. A full refresh still runs after a reset and every 12 hours. Wakes take 1.9 to 2.5 s instead of about 4.6 s.

## v1.0.2 · patch · 2026-11-05 (was v1.03)

- Reports ask the server for its RSA certificate, which the chip checks in hardware, instead of the ECDSA one it checked in software. Wakes take about 4.7 s instead of 5.7 s.

## v1.0.1 · patch · 2026-11-04 (was v1.01)

- The board draws the weather before downloading an update instead of after, so the screen no longer looks stuck for most of a minute.

## v1.0.0 · major · 2026-11-04 (was v1)

- First release. Firmware arrives over Wi-Fi from the server instead of only over USB, and a board goes back to its previous firmware if the new one fails to reach the server and redraw the screen. Boards flashed before it need one USB upload.
- The board checks the server's certificate instead of trusting any.
