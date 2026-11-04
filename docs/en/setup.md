[English](setup.md) | [한국어](../ko/setup.md)

# Setup and registration

## Build and upload
```bash
pio run -e weather-xiao -t upload
pio device monitor -b 115200
```
Once the deep-sleep firmware is on the board, a re-upload needs download mode: hold BOOT, tap RESET.

## Wi-Fi
Nothing to edit before building: the board asks for Wi-Fi itself. On a board with no stored
network it puts up an access point and shows the instructions on the e-Paper:

1. Join the Wi-Fi network `XIAO-ESP32-C3` from a phone
2. Open `192.168.4.1` in a browser and pick your network

Credentials go into the Wi-Fi driver's own NVS namespace, so they survive firmware uploads.

To change networks later: the board offers the page again after five failed connections, and
pressing RESET five times in a row, faster than a connect takes, reaches the same point
immediately. After five minutes with nobody configuring anything it goes back to sleep and keeps
whatever was stored.

## Registration
The server sends weather only to a board registered to an account.

1. Once Wi-Fi works, a board the server does not know shows a code and its QR, then sleeps with no
   wake timer. Nothing keeps checking in between
2. Scan the QR. It opens the dashboard with the code filled in; log in and register the board. The
   code can also go to Claude or ChatGPT, which register it over MCP
3. Press RESET. That wake asks the server again and draws the weather

- The code stays the same until it is used. If the server dropped it after its hour, the next ask
  puts it back
- When the owner releases a board, the server no longer knows its token, and the board shows a new
  code on its next wake
