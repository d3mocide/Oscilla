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

/* Rebuild the already-configured, unassociated STA driver after any other
 * engine that shares the C5's one PHY (promiscuous Wi-Fi, or 802.15.4 —
 * esp_ieee802154_enable/disable share the same esp_phy modem state) has
 * released it. A stop+start bounce of the STA driver is what actually
 * clears the PHY into a state plain Wi-Fi scan/config calls can use again;
 * skipping it after 802.15.4 use left Wi-Fi silently returning zero
 * results (2026-09-21, see WORKLOG). */
esp_err_t wifi_recon_restore_shared_phy(void);

/* Copy stored result `idx` (1-based). False if absent or a scan is running. */
bool wifi_recon_lookup(unsigned idx, uint8_t bssid[6], uint8_t *channel);

/* Command handlers: each emits its own reply (frame or [ERR]). */
void wifi_cmd_scan(void);
void wifi_cmd_show_results(int argc, char **argv);

#endif /* OSCILLA_WIFI_RECON_H */
