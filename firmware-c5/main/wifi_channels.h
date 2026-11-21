/*
 * wifi_channels.h — the shared passive channel-hop list, used by every
 * promiscuous engine (wifi_sniff.c, wifi_deauth.c, wifi_spectrum.c): plain
 * round-robin, both bands.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_CHANNELS_H
#define OSCILLA_WIFI_CHANNELS_H

#include <stddef.h>
#include <stdint.h>

#include "ocp_wifi_regions.h"

#define WIFI_CHAN_DWELL_MS 300   /* per channel */

/*
 * The hop list is the active Wi-Fi region's (protocol/ocp_wifi_regions.h):
 * the non-DFS channels ESP-IDF's regulatory database allows for that
 * country. Default US (1-11, 36-48, 149-165) until the deck sends
 * `set_region`. DFS channels (52-144) stay out of every region (D-14).
 * The ESP-IDF country must match (wifi_recon_set_country), or
 * esp_wifi_set_channel() rejects channels outside it with
 * ESP_ERR_INVALID_ARG, which the hop callbacks log.
 *
 * D-UCB dwell weighting is deferred (D-6); this is v1's "simple
 * round-robin".
 */
const ocp_wifi_region_t *wifi_region(void);

/* Only while no PHY tool runs: engines read the list once at start. */
void wifi_region_select(const ocp_wifi_region_t *region);

/* The active region's hop list. Static storage, valid for the process
 * lifetime; re-read at every engine start. */
const uint8_t *wifi_channels(size_t *count);

#endif /* OSCILLA_WIFI_CHANNELS_H */
