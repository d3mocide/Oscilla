/*
 * wifi_wardrive.h — probe-driven passive multi-band wardrive engine
 * (`start_wardrive`, DESIGN §6.2, OCP-SPEC §10.10).
 *
 * Holds a single combined PHY owner (PHY_OWNER_WARDRIVE), time-slicing
 * passive Wi-Fi channel hopping with duty-cycled passive BLE sweeps and
 * bounded TTL/LRU dedup so moving drives re-emit observations without
 * UART flooding. Strictly receive-only (D-8).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_WARDRIVE_H
#define OSCILLA_WIFI_WARDRIVE_H

#include "esp_err.h"

esp_err_t wifi_wardrive_init(void);

/* start_wardrive [ble_0_or_1] [wifi_dwell_ms] [ble_dwell_ms] [ttl_s] */
void wifi_cmd_start_wardrive(int argc, char **argv);

#endif /* OSCILLA_WIFI_WARDRIVE_H */
