/*
 * ble_recon.h — BLE passive scanning over NimBLE (OCP-SPEC §11): device
 * discovery (`scan_bt`) and continuous Find My / AirTag classification
 * (`scan_airtag`). Thin glue over ble_adv_parse.h (parsing) and
 * ble_device_table.h (the table `scan_bt` replies with) — NimBLE host
 * plumbing, the radio_arbiter, and OCP framing live here, nothing else.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_BLE_RECON_H
#define OSCILLA_BLE_RECON_H

#include <stdbool.h>
#include "esp_err.h"

#define BLE_SCAN_DWELL_MS   6000   /* default scan_bt window */

/* Starts the NimBLE host (async: ble_recon_ready() flips true once it has
 * actually synced with the controller, not merely been asked to start —
 * DESIGN §6.1's "a radio that fails to come up stays local" applies the
 * same way wifi_recon_init()/lora_recon_init() do). */
esp_err_t ble_recon_init(void);
bool ble_recon_ready(void);

/* `argv[1]`, if present, overrides BLE_SCAN_DWELL_MS (milliseconds). Replies
 * with the [BLE] device table once the window closes — never blocks the
 * dispatch task; the reply comes from the NimBLE host task's completion
 * callback (OCP-SPEC §11.2). */
void ble_cmd_scan_bt(int argc, char **argv);

/* Runs until `stop`; streams [EVT] kind=airtag for every Find My / AirTag
 * advertisement seen, regardless of what scan_bt has or hasn't found
 * (OCP-SPEC §11.3). */
void ble_cmd_scan_airtag(void);

#endif /* OSCILLA_BLE_RECON_H */
