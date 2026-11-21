/*
 * wifi_channels.h — the shared passive channel-hop list, used by every
 * promiscuous engine (wifi_sniff.c, wifi_deauth.c, wifi_spectrum.c,
 * wifi_networks.c) and passive scan bitmap filtering.
 *
 * Pure C, no ESP-IDF dependency, so it is host-tested under ASan/UBSan.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_CHANNELS_H
#define OSCILLA_WIFI_CHANNELS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ocp_wifi_regions.h"

#define WIFI_CHAN_DWELL_MS 300   /* per channel */

typedef enum {
    WIFI_BAND_SCOPE_AUTO = 0,
    WIFI_BAND_SCOPE_24,
    WIFI_BAND_SCOPE_5,
} wifi_band_scope_t;

/*
 * The active hop list derives from the active Wi-Fi region
 * (protocol/ocp_wifi_regions.h, §10.8), filtered by the active band scope
 * and optional custom channel list (§10.9). Default US + auto + all.
 * DFS channels (52-144) stay out of every region (D-14).
 *
 * Mutators below are called only while no PHY tool runs; engines re-read
 * wifi_channels() at every start.
 */
const ocp_wifi_region_t *wifi_region(void);
void wifi_region_select(const ocp_wifi_region_t *region);

wifi_band_scope_t wifi_band_scope(void);
const char *wifi_band_scope_name(wifi_band_scope_t scope);
bool wifi_band_scope_parse(const char *s, wifi_band_scope_t *out);
void wifi_band_select(wifi_band_scope_t scope);

/* OCP-SPEC §10.9: "all" resets to (region, band); a comma-separated list
 * must contain distinct channels inside both the active region and band.
 * Returns false without mutating state on any invalid input. */
bool wifi_channels_set(const char *arg);
bool wifi_channels_is_custom(void);

/* The active hop list (always >= 1 channel). Static storage, valid for the
 * process lifetime; re-read at every engine start. */
const uint8_t *wifi_channels(size_t *count);

/* Format the active hop list as comma-separated decimals for `chans=`. */
void wifi_channels_format_list(char *out, size_t n);

/* Restore boot defaults (US, auto, all) — primarily for host tests. */
void wifi_channels_reset(void);

#endif /* OSCILLA_WIFI_CHANNELS_H */
