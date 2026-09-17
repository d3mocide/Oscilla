/*
 * wifi_network_table.h — bounded AP/BSSID table for continuous passive
 * discovery. Pure C: the promiscuous callback and host tests share this
 * deduplication logic.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_WIFI_NETWORK_TABLE_H
#define OSCILLA_WIFI_NETWORK_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "beacon_parse.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_NETWORKS_MAX 256

typedef struct {
    uint8_t  bssid[6];
    uint8_t  ssid[32];
    uint8_t  ssid_len;
    bool     has_ssid;
    uint8_t  channel;
    int8_t   rssi;
    bool     privacy;
    bool     has_rsn;
    bool     mfp_capable;
    bool     mfp_required;
    uint16_t interval_ms;
    uint32_t seen;
} wifi_network_t;

typedef struct {
    wifi_network_t networks[WIFI_NETWORKS_MAX];
    unsigned count;
} wifi_network_table_t;

void wifi_network_table_reset(wifi_network_table_t *t);

/* Upserts by BSSID, updating the latest observation in place. Returns true
 * with `out` filled only for a genuinely new BSSID; false means an existing
 * row was updated or the bounded table rejected a new row. */
bool wifi_network_table_upsert(wifi_network_table_t *t, const beacon_info_t *info,
                               uint8_t channel, int8_t rssi, wifi_network_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_WIFI_NETWORK_TABLE_H */
