#pragma once
#include <Arduino.h>

// Whether Wi-Fi credentials are already stored. The Wi-Fi driver keeps them in its own NVS
// namespace, so they survive a firmware upload and there is nothing to write here.
bool wifiProvisioned();

// Bring up an access point and serve the setup page until credentials are saved or the
// timeout runs out. Blocks for up to timeoutSec. Returns whether credentials were saved.
bool runProvisioningPortal(const char* apName, uint16_t timeoutSec);

// Same page, but without discarding what is already stored — for when the saved network has
// stopped answering. If nobody configures anything, the old credentials are still there and
// the board can reconnect on its own once the router comes back.
bool runRecoveryPortal(const char* apName, uint16_t timeoutSec);
