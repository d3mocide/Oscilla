/*
 * sniff_track.h — the promiscuous sniffer's tracking tables: AP<->client
 * pairings from data-frame address fields, and mac+SSID pairings from probe
 * requests (OCP-SPEC §10.4).
 *
 * Pure C, no IDF dependency: address-field extraction and table upsert are
 * the actual logic worth getting wrong, so they're host-testable and fuzzed
 * (firmware-c5/test/host/sniff_track_test.c) the same way beacon_parse.c and
 * probe_parse.c are. wifi_sniff.c is thin glue over this: promiscuous
 * callback registration, the channel hopper, the arbiter, and OCP framing.
 *
 * Not thread-safe; the caller (wifi_sniff.c) owns the lock.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_SNIFF_TRACK_H
#define OSCILLA_SNIFF_TRACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SNIFF_TRACK_CLIENTS_MAX   128   /* one [CLIENTS] frame, no paging (§10.4) */
#define SNIFF_TRACK_PROBES_MAX    192   /* one [PROBES] frame, no paging (§10.4) */

typedef struct {
    uint8_t  bssid[6];
    uint8_t  mac[6];
    uint8_t  channel;
    int8_t   rssi;
    uint32_t pkts;
} sniff_client_t;

typedef struct {
    uint8_t  mac[6];
    uint8_t  ssid_len;      /* 0 for a wildcard/broadcast probe */
    uint8_t  ssid[32];
    int8_t   rssi;
    uint32_t pkts;
} sniff_probe_t;

typedef struct {
    sniff_client_t clients[SNIFF_TRACK_CLIENTS_MAX];
    unsigned       client_count;
    sniff_probe_t  probes[SNIFF_TRACK_PROBES_MAX];
    unsigned       probe_count;
} sniff_track_t;

void sniff_track_reset(sniff_track_t *t);

/* Extracts the AP/client pairing from a data frame's address fields
 * (ToDS/FromDS distinguishes which is which) and upserts it. `frame`/`len`
 * are the 802.11 MAC header and beyond, `len` excluding the FCS. False (no
 * `out`) for: a frame shorter than a MAC header, IBSS (ToDS=FromDS=0) or WDS
 * (ToDS=FromDS=1) framing (no single AP/client pairing to record), a
 * multicast station or BSSID, or a table already at
 * SNIFF_TRACK_CLIENTS_MAX with no matching existing row (lossy by design).
 * True with `out` filled iff this pairing was not already stored. */
bool sniff_track_data_frame(sniff_track_t *t, const uint8_t *frame, size_t len,
                            uint8_t channel, int8_t rssi, sniff_client_t *out);

/* Upserts an already-parsed probe request (probe_parse.h). False (no `out`)
 * for a multicast/malformed transmitter address or a full table with no
 * matching row. True with `out` filled iff this mac+SSID pairing was not
 * already stored. */
bool sniff_track_probe(sniff_track_t *t, const uint8_t mac[6], const uint8_t *ssid,
                       uint8_t ssid_len, int8_t rssi, sniff_probe_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_SNIFF_TRACK_H */
