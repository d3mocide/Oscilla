/*
 * wifi_network_table.c — see wifi_network_table.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_network_table.h"

#include <string.h>

static bool mac_is_multicast(const uint8_t *m) { return (m[0] & 0x01) != 0; }

void wifi_network_table_reset(wifi_network_table_t *t)
{
    memset(t, 0, sizeof *t);
}

static void update(wifi_network_t *n, const beacon_info_t *info, uint8_t channel, int8_t rssi)
{
    if (info->has_ssid) {
        n->has_ssid = true;
        n->ssid_len = info->ssid_len;
        memcpy(n->ssid, info->ssid, info->ssid_len);
    }
    n->channel = channel;                    /* the hopper's channel is authoritative */
    n->rssi = rssi;
    n->privacy = (info->capability & (1u << 4)) != 0;
    n->has_rsn = info->has_rsn && !info->rsn_malformed;
    n->mfp_capable = n->has_rsn && info->mfp_capable;
    n->mfp_required = n->has_rsn && info->mfp_required;
    n->interval_ms = (uint16_t)((info->interval_tu * 1024u + 500u) / 1000u);
    n->seen++;
}

bool wifi_network_table_upsert(wifi_network_table_t *t, const beacon_info_t *info,
                               uint8_t channel, int8_t rssi, wifi_network_t *out)
{
    if (!info || mac_is_multicast(info->bssid)) return false;

    for (unsigned i = 0; i < t->count; i++) {
        wifi_network_t *n = &t->networks[i];
        if (memcmp(n->bssid, info->bssid, sizeof n->bssid) == 0) {
            update(n, info, channel, rssi);
            return false;
        }
    }

    if (t->count >= WIFI_NETWORKS_MAX) return false;

    wifi_network_t *n = &t->networks[t->count++];
    memset(n, 0, sizeof *n);
    memcpy(n->bssid, info->bssid, sizeof n->bssid);
    update(n, info, channel, rssi);
    if (out) *out = *n;
    return true;
}
