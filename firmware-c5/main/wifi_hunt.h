/*
 * wifi_hunt.h — single-target passive Wi-Fi Signal Hunt engine (WP-12b).
 *
 * Locks promiscuous RX onto one validated channel and streams rate-limited
 * [EVT] kind=hunt RSSI readings for frames transmitted by a target BSSID/MAC.
 * Strictly receive-only (D-8).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_HUNT_H
#define OSCILLA_WIFI_HUNT_H

#include "esp_err.h"

esp_err_t wifi_hunt_init(void);

/* hunt_wifi <mac> <ch> */
void wifi_cmd_hunt(int argc, char **argv);

#endif /* OSCILLA_WIFI_HUNT_H */
