#pragma once
#include <Arduino.h>

// Whether Wi-Fi credentials are already stored. The Wi-Fi driver keeps them in its own NVS
// namespace, so they survive a firmware upload and there is nothing to write here.
bool wifiProvisioned();

// Put up an access point and serve the setup page. Blocks for up to timeoutSec. Returns whether
// credentials were saved. Nothing already stored is erased, so a portal nobody answers leaves
// the board able to reconnect once the router comes back.
bool runSetupPortal(const char* apName, uint16_t timeoutSec);
