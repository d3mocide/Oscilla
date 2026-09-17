/*
 * wifi_recon.h — Wi-Fi survey engine: passive scan, paging (OCP-SPEC §10).
 *
 * Receive-only: scans are passive and never send probe requests (§10.1).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_RECON_H
#define OSCILLA_WIFI_RECON_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define WIFI_SCAN_DWELL_MS   250    /* per channel; beacons arrive every ~102 ms */
#define WIFI_STORE_MAX       256    /* deck persists larger field captures */

/* Brings up Wi-Fi in station mode without associating. */
esp_err_t wifi_recon_init(void);
bool wifi_recon_ready(void);

/* Copy stored result `idx` (1-based). False if absent or a scan is running. */
bool wifi_recon_lookup(unsigned idx, uint8_t bssid[6], uint8_t *channel);

/* Command handlers: each emits its own reply (frame or [ERR]). */
void wifi_cmd_scan(void);
void wifi_cmd_show_results(int argc, char **argv);

#endif /* OSCILLA_WIFI_RECON_H */
