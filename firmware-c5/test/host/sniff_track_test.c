/*
 * sniff_track_test.c — known-answer tests and a mutation fuzzer for
 * sniff_track. Build with -fsanitize=address,undefined: any out-of-bounds
 * read aborts.
 *
 * This is the piece that can be verified with no radio, no hardware, no RF
 * environment: given the address fields a promiscuous callback would hand
 * it, does the tracker classify STA vs. BSSID correctly, dedup correctly,
 * and stay bounded? What still needs a real probe on the bench is whether
 * real over-the-air frames reach the callback at all (tools/ocp_repl.py
 * --gate-sniffer).
 *
 * SPDX-License-Identifier: MIT
 */

#include "sniff_track.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, what) do { \
    if (cond) g_pass++; else { g_fail++; printf("  FAIL  %s (line %d)\n", what, __LINE__); } \
} while (0)

typedef struct { uint8_t b[24]; } hdr_t;

/* A minimal 24-byte 802.11 MAC header: FC byte1 carries ToDS/FromDS. */
static hdr_t header(uint8_t tods, uint8_t fromds, const uint8_t *a1, const uint8_t *a2, const uint8_t *a3)
{
    hdr_t f;
    memset(&f, 0, sizeof f);
    f.b[0] = 0x08;                                    /* type=data */
    f.b[1] = (uint8_t)((tods & 1) | ((fromds & 1) << 1));
    memcpy(f.b + 4, a1, 6);
    memcpy(f.b + 10, a2, 6);
    memcpy(f.b + 16, a3, 6);
    return f;
}

static const uint8_t k_bssid[6] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01 };
static const uint8_t k_sta[6]   = { 0xf4, 0x12, 0x34, 0x56, 0x78, 0x9a };
static const uint8_t k_mcast[6] = { 0x01, 0x00, 0x5e, 0x00, 0x00, 0x01 };
static const uint8_t k_bcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static void known_answers_client(void)
{
    sniff_track_t t;
    sniff_client_t out;

    /* STA -> AP: ToDS=1, addr1=BSSID, addr2=STA. */
    sniff_track_reset(&t);
    hdr_t f = header(1, 0, k_bssid, k_sta, k_bcast);
    CHECK(sniff_track_data_frame(&t, f.b, sizeof f.b, 6, -50, &out) &&
          memcmp(out.bssid, k_bssid, 6) == 0 && memcmp(out.mac, k_sta, 6) == 0,
          "STA->AP: bssid=addr1, mac=addr2");

    /* AP -> STA: FromDS=1, addr1=STA, addr2=BSSID. */
    sniff_track_reset(&t);
    f = header(0, 1, k_sta, k_bssid, k_bssid);
    CHECK(sniff_track_data_frame(&t, f.b, sizeof f.b, 6, -50, &out) &&
          memcmp(out.bssid, k_bssid, 6) == 0 && memcmp(out.mac, k_sta, 6) == 0,
          "AP->STA: bssid=addr2, mac=addr1");

    /* IBSS and WDS: no single pairing, not tracked. */
    sniff_track_reset(&t);
    f = header(0, 0, k_sta, k_bssid, k_bssid);
    CHECK(!sniff_track_data_frame(&t, f.b, sizeof f.b, 6, -50, &out) && t.client_count == 0,
          "ToDS=0,FromDS=0 (IBSS): not tracked");
    f = header(1, 1, k_sta, k_bssid, k_bssid);
    CHECK(!sniff_track_data_frame(&t, f.b, sizeof f.b, 6, -50, &out) && t.client_count == 0,
          "ToDS=1,FromDS=1 (WDS): not tracked");

    /* Multicast station or BSSID: not a real client, not tracked. */
    sniff_track_reset(&t);
    f = header(1, 0, k_bssid, k_mcast, k_bcast);
    CHECK(!sniff_track_data_frame(&t, f.b, sizeof f.b, 6, -50, &out), "multicast STA rejected");
    f = header(1, 0, k_mcast, k_sta, k_bcast);
    CHECK(!sniff_track_data_frame(&t, f.b, sizeof f.b, 6, -50, &out), "multicast BSSID rejected");

    /* Dedup: same pairing again updates in place, doesn't grow the table. */
    sniff_track_reset(&t);
    f = header(1, 0, k_bssid, k_sta, k_bcast);
    CHECK(sniff_track_data_frame(&t, f.b, sizeof f.b, 6, -50, &out), "first sighting is new");
    CHECK(!sniff_track_data_frame(&t, f.b, sizeof f.b, 1, -40, NULL) && t.client_count == 1,
          "second sighting updates in place, table doesn't grow");
    CHECK(t.clients[0].pkts == 2 && t.clients[0].channel == 1 && t.clients[0].rssi == -40,
          "pkts/channel/rssi updated on the existing row");

    /* Too short: below the 24-byte MAC header. */
    sniff_track_reset(&t);
    CHECK(!sniff_track_data_frame(&t, f.b, 23, 6, -50, &out), "23 bytes: too short to have addresses");

    /* Table overflow: capped, existing rows still work, no crash. */
    sniff_track_reset(&t);
    uint8_t sta[6];
    memcpy(sta, k_sta, 6);
    for (unsigned i = 0; i < SNIFF_TRACK_CLIENTS_MAX; i++) {
        sta[5] = (uint8_t)i;
        f = header(1, 0, k_bssid, sta, k_bcast);
        CHECK(sniff_track_data_frame(&t, f.b, sizeof f.b, 6, -50, &out), "fills the table one by one");
    }
    CHECK(t.client_count == SNIFF_TRACK_CLIENTS_MAX, "table capped at SNIFF_TRACK_CLIENTS_MAX");
    sta[5] = (uint8_t)SNIFF_TRACK_CLIENTS_MAX;   /* one more, distinct pairing */
    f = header(1, 0, k_bssid, sta, k_bcast);
    CHECK(!sniff_track_data_frame(&t, f.b, sizeof f.b, 6, -50, &out) && t.client_count == SNIFF_TRACK_CLIENTS_MAX,
          "a full table drops new pairings rather than growing (lossy by design)");
    sta[5] = 0;
    f = header(1, 0, k_bssid, sta, k_bcast);
    CHECK(!sniff_track_data_frame(&t, f.b, sizeof f.b, 6, -50, &out) && t.clients[0].pkts == 2,
          "an existing row still updates once the table is full");
}

static void known_answers_probe(void)
{
    sniff_track_t t;
    sniff_probe_t out;
    const uint8_t ssid_a[] = "HomeNet";
    const uint8_t ssid_b[] = "Office";

    sniff_track_reset(&t);
    CHECK(sniff_track_probe(&t, k_sta, ssid_a, 7, -60, &out) &&
          memcmp(out.mac, k_sta, 6) == 0 && out.ssid_len == 7 && memcmp(out.ssid, ssid_a, 7) == 0,
          "directed probe recorded");

    /* Same mac, different SSID: a second, distinct row (a device asking for
     * two saved networks is exactly the tracking signal this exists for). */
    CHECK(sniff_track_probe(&t, k_sta, ssid_b, 6, -60, &out) && t.probe_count == 2,
          "same mac, different SSID: separate row");

    /* Same mac+SSID again: dedup in place. */
    CHECK(!sniff_track_probe(&t, k_sta, ssid_a, 7, -55, NULL) && t.probe_count == 2,
          "same mac+SSID again: updates in place");
    CHECK(t.probes[0].pkts == 2 && t.probes[0].rssi == -55, "pkts/rssi updated");

    /* Wildcard probe: ssid_len=0 is a valid, trackable pairing, not dropped.
     * (A non-null pointer even at length 0: memcmp/memcpy with NULL is UB,
     * and probe_req_info_t's ssid field is never actually NULL in practice.) */
    const uint8_t empty[1] = { 0 };
    CHECK(sniff_track_probe(&t, k_sta, empty, 0, -60, &out) && out.ssid_len == 0,
          "wildcard (empty SSID) probe is tracked, distinct from a directed one");

    /* Multicast/broadcast transmitter address: never a real device. */
    CHECK(!sniff_track_probe(&t, k_mcast, ssid_a, 7, -60, &out), "multicast transmitter rejected");

    /* Oversized SSID length is clamped defensively, even though probe_parse
     * already caps at 32 before calling in. */
    uint8_t big[40]; memset(big, 'S', sizeof big);
    sniff_track_reset(&t);
    CHECK(sniff_track_probe(&t, k_sta, big, 40, -60, &out) && out.ssid_len == 32,
          "over-length SSID clamped to the row's capacity");

    /* Table overflow. */
    sniff_track_reset(&t);
    uint8_t mac[6];
    memcpy(mac, k_sta, 6);
    for (unsigned i = 0; i < SNIFF_TRACK_PROBES_MAX; i++) {
        mac[5] = (uint8_t)i;
        CHECK(sniff_track_probe(&t, mac, ssid_a, 7, -60, &out), "fills the probe table one by one");
    }
    mac[5] = (uint8_t)SNIFF_TRACK_PROBES_MAX;
    CHECK(!sniff_track_probe(&t, mac, ssid_a, 7, -60, &out) && t.probe_count == SNIFF_TRACK_PROBES_MAX,
          "a full probe table drops new pairings (lossy by design)");
}

static uint32_t rng = 0x5A17FEED;
static uint32_t next(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

/* Random bytes through sniff_track_data_frame, exact-size heap copies so
 * ASan flags even a 1-byte over-read past a short/garbage frame. */
static void fuzz(long iterations)
{
    sniff_track_t t;
    sniff_track_reset(&t);

    for (long i = 0; i < iterations; i++) {
        uint8_t buf[64];
        size_t n = next() % (sizeof buf + 1);
        for (size_t k = 0; k < n; k++) buf[k] = (uint8_t)next();

        uint8_t *exact = malloc(n ? n : 1);
        memcpy(exact, buf, n);
        sniff_client_t co;
        sniff_track_data_frame(&t, exact, n, (uint8_t)(next() % 200), (int8_t)next(), &co);
        free(exact);

        uint8_t mac[6], ssid[40];
        for (int k = 0; k < 6; k++) mac[k] = (uint8_t)next();
        uint8_t slen = (uint8_t)(next() % sizeof ssid);
        for (int k = 0; k < slen; k++) ssid[k] = (uint8_t)next();
        sniff_probe_t po;
        sniff_track_probe(&t, mac, ssid, slen, (int8_t)next(), &po);
        if (t.client_count > SNIFF_TRACK_CLIENTS_MAX || t.probe_count > SNIFF_TRACK_PROBES_MAX) {
            g_fail++;
            printf("  FAIL  table grew past its cap\n");
            return;
        }
    }
    g_pass++;
    printf("  PASS  fuzz: %ld rounds, no over-read, tables stay bounded (ASan/UBSan)\n", iterations);
}

int main(int argc, char **argv)
{
    long iterations = argc > 1 ? atol(argv[1]) : 200000;
    known_answers_client();
    known_answers_probe();
    fuzz(iterations);
    printf("\n%s: %d passed, %d failed\n", g_fail ? "sniff_track test FAILED" : "sniff_track test OK", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
