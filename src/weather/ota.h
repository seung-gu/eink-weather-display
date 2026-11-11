#pragma once
#include <Arduino.h>

// Firmware updates over Wi-Fi. When a release newer than this build exists, the server adds
// "ota_version" (the release commit) to the weather answer. otaUpdate() then downloads that
// release from SERVER_HOST into the other app slot and restarts into it.
//
// The new firmware starts on probation. If it crashes, or resets for any reason before
// otaConfirm(), the bootloader goes back to the previous one on the next boot. That covers a
// release that cannot reach the server or cannot draw the screen — the two things a wake is for.
// The old firmware sees the same offer again and tries on its next scheduled wake. After OTA_TRIES
// it gives up, says so in its report, and the server turns auto update off until the owner turns
// it back on.

// Called with this wake's 200 weather body, after the screen is drawn. Connects Wi-Fi again only
// when there is something to install. Returns when there is nothing to install or the download
// failed; after a successful install it restarts and does not return.
void otaUpdate(const String& weatherBody);

// Keeps the running firmware for good. Called at the end of a wake that reached the server.
void otaConfirm();

// True once the offered update has had all its tries without taking. The report carries it.
bool otaGaveUp();
