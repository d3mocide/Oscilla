/*
 * sniff_track.c — see sniff_track.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "sniff_track.h"

#include <string.h>

static bool mac_is_multicast(const uint8_t *m) { return (m[0] & 0x01) != 0; }

void sniff_track_reset(sniff_track_t *t)
{
    memset(t, 0, sizeof *t);
}

bool sniff_track_data_frame(sniff_track_t *t, const uint8_t *frame, size_t len,
                            uint8_t channel, int8_t rssi, sniff_client_t *out)
{
    if (len < 24) return false;

    uint8_t tods = frame[1] & 0x01, fromds = (frame[1] >> 1) & 0x01;
    const uint8_t *bssid, *sta;
    if (tods && !fromds) { bssid = frame + 4; sta = frame + 10; }          /* STA -> AP */
    else if (!tods && fromds) { sta = frame + 4; bssid = frame + 10; }     /* AP -> STA */
    else return false;   /* IBSS (0,0) or WDS (1,1): no single pairing here */

    if (mac_is_multicast(sta) || mac_is_multicast(bssid)) return false;

    for (unsigned i = 0; i < t->client_count; i++) {
        sniff_client_t *c = &t->clients[i];
        if (memcmp(c->bssid, bssid, 6) == 0 && memcmp(c->mac, sta, 6) == 0) {
            c->channel = channel;
            c->rssi = rssi;
            c->pkts++;
            return false;
        }
    }
    if (t->client_count >= SNIFF_TRACK_CLIENTS_MAX) return false;   /* full: lossy by design */

    sniff_client_t *c = &t->clients[t->client_count++];
    memcpy(c->bssid, bssid, 6);
    memcpy(c->mac, sta, 6);
    c->channel = channel;
    c->rssi = rssi;
    c->pkts = 1;
    if (out) *out = *c;
    return true;
}

bool sniff_track_probe(sniff_track_t *t, const uint8_t mac[6], const uint8_t *ssid,
                       uint8_t ssid_len, int8_t rssi, sniff_probe_t *out)
{
    if (mac_is_multicast(mac)) return false;
    if (ssid_len > sizeof t->probes[0].ssid) ssid_len = sizeof t->probes[0].ssid;

    for (unsigned i = 0; i < t->probe_count; i++) {
        sniff_probe_t *p = &t->probes[i];
        if (memcmp(p->mac, mac, 6) == 0 && p->ssid_len == ssid_len &&
            memcmp(p->ssid, ssid, ssid_len) == 0) {
            p->rssi = rssi;
            p->pkts++;
            return false;
        }
    }
    if (t->probe_count >= SNIFF_TRACK_PROBES_MAX) return false;    /* full: lossy by design */

    sniff_probe_t *p = &t->probes[t->probe_count++];
    memcpy(p->mac, mac, 6);
    p->ssid_len = ssid_len;
    memcpy(p->ssid, ssid, ssid_len);
    p->rssi = rssi;
    p->pkts = 1;
    if (out) *out = *p;
    return true;
}
