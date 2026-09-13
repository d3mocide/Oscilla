/*
 * beacon_test.c — known-answer tests and a mutation fuzzer for beacon_parse.
 * Build with -fsanitize=address,undefined: any out-of-bounds read aborts.
 *
 * SPDX-License-Identifier: MIT
 */

#include "beacon_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, what) do { \
    if (cond) g_pass++; else { g_fail++; printf("  FAIL  %s (line %d)\n", what, __LINE__); } \
} while (0)

typedef struct { uint8_t b[512]; size_t n; } frame_t;

static void put(frame_t *f, const uint8_t *p, size_t n) { memcpy(f->b + f->n, p, n); f->n += n; }
static void put8(frame_t *f, uint8_t v) { f->b[f->n++] = v; }
static void put16(frame_t *f, uint16_t v) { put8(f, v & 0xFF); put8(f, v >> 8); }

static void header(frame_t *f, uint8_t fc, uint64_t tsf, uint16_t interval)
{
    static const uint8_t bssid[6] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01 };
    memset(f, 0, sizeof *f);
    put8(f, fc); put8(f, 0);                         /* frame control */
    put16(f, 0);                                     /* duration */
    for (int i = 0; i < 6; i++) put8(f, 0xff);       /* DA broadcast */
    put(f, bssid, 6);                                /* SA */
    put(f, bssid, 6);                                /* BSSID */
    put16(f, 0);                                     /* seq */
    for (int i = 0; i < 8; i++) put8(f, (uint8_t)(tsf >> (8 * i)));
    put16(f, interval);
    put16(f, 0x0431);                                /* capability */
}

static void ie(frame_t *f, uint8_t id, const uint8_t *p, uint8_t n) { put8(f, id); put8(f, n); put(f, p, n); }

/* Parse from an exact-size heap copy, so ASan flags even a 1-byte over-read. */
static beacon_result_t parse_exact(const uint8_t *b, size_t n, beacon_info_t *bi)
{
    uint8_t *copy = malloc(n ? n : 1);
    memcpy(copy, b, n);
    beacon_result_t r = beacon_parse(copy, n, bi);
    free(copy);
    return r;
}

/* RSN: version 1, CCMP group, 1 pairwise CCMP, 1 AKM, capabilities. */
static void rsn(frame_t *f, uint16_t caps)
{
    uint8_t r[] = { 1, 0,  0x00, 0x0f, 0xac, 4,  1, 0, 0x00, 0x0f, 0xac, 4,
                    1, 0,  0x00, 0x0f, 0xac, 2,  (uint8_t)caps, (uint8_t)(caps >> 8) };
    ie(f, 48, r, sizeof r);
}

static void known_answers(void)
{
    frame_t f;
    beacon_info_t bi;

    header(&f, 0x80, 1042311000000ULL, 100);
    uint8_t ssid[] = "HomeNet";
    ie(&f, 0, ssid, 7);
    uint8_t ds[] = { 6 };
    ie(&f, 3, ds, 1);
    rsn(&f, 0x0080);                                 /* MFPC only */
    CHECK(parse_exact(f.b, f.n, &bi) == BEACON_OK, "valid beacon parses");
    CHECK(bi.tsf_us == 1042311000000ULL, "TSF little-endian");
    CHECK(bi.interval_tu == 100, "beacon interval");
    CHECK(bi.bssid[0] == 0xaa && bi.bssid[5] == 0x01, "BSSID from addr3");
    CHECK(bi.has_ssid && bi.ssid_len == 7 && memcmp(bi.ssid, "HomeNet", 7) == 0, "SSID");
    CHECK(bi.ds_channel == 6, "DS channel");
    CHECK(bi.has_rsn && !bi.rsn_malformed && bi.mfp_capable && !bi.mfp_required, "MFPC=1 MFPR=0");

    header(&f, 0x80, 0, 100); rsn(&f, 0x00C0);
    parse_exact(f.b, f.n, &bi);
    CHECK(bi.mfp_capable && bi.mfp_required, "MFPC=1 MFPR=1 (WPA3-only)");

    header(&f, 0x50, 0, 100); rsn(&f, 0x0000);       /* probe response */
    CHECK(parse_exact(f.b, f.n, &bi) == BEACON_OK && !bi.mfp_capable, "probe response accepted, MFP off");

    header(&f, 0x80, 0, 100);
    CHECK(parse_exact(f.b, f.n, &bi) == BEACON_OK && !bi.has_rsn && !bi.has_ssid, "open network: no RSN");

    header(&f, 0x80, 0, 100);
    uint8_t short_rsn[] = { 1, 0, 0x00, 0x0f, 0xac, 4 };   /* version + group only */
    ie(&f, 48, short_rsn, sizeof short_rsn);
    parse_exact(f.b, f.n, &bi);
    CHECK(bi.has_rsn && !bi.rsn_malformed && !bi.mfp_capable, "RSN truncated at a field boundary is valid");

    header(&f, 0x80, 0, 100);
    uint8_t lying_rsn[] = { 1, 0, 0x00, 0x0f, 0xac, 4, 0xff, 0xff, 0x00, 0x0f };
    ie(&f, 48, lying_rsn, sizeof lying_rsn);
    parse_exact(f.b, f.n, &bi);
    CHECK(bi.has_rsn && bi.rsn_malformed && !bi.mfp_capable, "RSN pairwise count past the element: malformed, no over-read");

    header(&f, 0x80, 0, 100);
    put8(&f, 0); put8(&f, 200); put8(&f, 'x');       /* SSID IE claims 200 bytes */
    CHECK(parse_exact(f.b, f.n, &bi) == BEACON_OK && bi.ies_truncated && !bi.has_ssid,
          "element running past the frame: truncated, not read");

    header(&f, 0x80, 0, 100);
    uint8_t big[64]; memset(big, 'S', sizeof big);
    ie(&f, 0, big, sizeof big);
    parse_exact(f.b, f.n, &bi);
    CHECK(bi.has_ssid && bi.ssid_len == 32, "over-long SSID clamped to 32");

    header(&f, 0x80, 0, 100);
    uint8_t one_cap_byte[] = { 1, 0, 0x00, 0x0f, 0xac, 4, 1, 0, 0x00, 0x0f, 0xac, 4,
                               1, 0, 0x00, 0x0f, 0xac, 2, 0xC0 };   /* caps needs 2 bytes, has 1 */
    ie(&f, 48, one_cap_byte, sizeof one_cap_byte);                   /* last element: ends at frame end */
    parse_exact(f.b, f.n, &bi);
    CHECK(bi.has_rsn && bi.rsn_malformed && !bi.mfp_capable, "half an RSN capabilities field: malformed, no over-read");

    header(&f, 0x80, 0, 100);
    ie(&f, 48, one_cap_byte, sizeof one_cap_byte);
    uint8_t after[] = { 0xFF, 0xFF }; ie(&f, 221, after, 2);           /* next element's bytes follow */
    parse_exact(f.b, f.n, &bi);
    CHECK(bi.rsn_malformed && !bi.mfp_required, "half a capabilities field never borrows the next element's bytes");

    header(&f, 0x80, 0, 100); rsn(&f, 0x0080); rsn(&f, 0x00C0);
    parse_exact(f.b, f.n, &bi);
    CHECK(bi.mfp_capable && !bi.mfp_required, "first RSN element wins");

    header(&f, 0x08, 0, 100);                        /* data frame */
    CHECK(parse_exact(f.b, f.n, &bi) == BEACON_NOT_MGMT_BEACON, "data frame rejected");
    header(&f, 0x40, 0, 100);                        /* probe request */
    CHECK(parse_exact(f.b, f.n, &bi) == BEACON_NOT_MGMT_BEACON, "probe request rejected");
    CHECK(parse_exact(f.b, 35, &bi) == BEACON_TOO_SHORT, "35 bytes: too short");
    CHECK(parse_exact(f.b, 0, &bi) == BEACON_TOO_SHORT, "empty: too short");
}

static uint32_t rng = 0xC5BEAC05u;
static uint32_t next(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static void fuzz(long iterations)
{
    beacon_info_t bi;
    frame_t seed;
    header(&seed, 0x80, 12345, 100);
    uint8_t ssid[] = "Fuzz"; ie(&seed, 0, ssid, 4);
    uint8_t ds[] = { 11 }; ie(&seed, 3, ds, 1);
    rsn(&seed, 0x00C0);

    for (long i = 0; i < iterations; i++) {
        uint8_t buf[512];
        size_t n;
        if (i % 3 == 0) {                            /* pure random */
            n = next() % sizeof buf;
            for (size_t k = 0; k < n; k++) buf[k] = (uint8_t)next();
            if (n && (next() & 1)) buf[0] = (next() & 1) ? 0x80 : 0x50;
        } else {                                     /* mutate a valid beacon */
            n = seed.n;
            memcpy(buf, seed.b, n);
            int muts = 1 + next() % 6;
            for (int m = 0; m < muts; m++) {
                switch (next() % 5) {
                case 4: {                                                     /* lie about an element length */
                    size_t off = 36, pick = next() % 4;
                    while (pick-- && off + 1 < n) off += 2 + buf[off + 1];
                    if (off + 1 < n) {
                        buf[off + 1] = (uint8_t)(buf[off + 1] + (int)(next() % 7) - 3);
                        if (next() & 1) n = off + 2 + buf[off + 1] < n ? off + 2 + buf[off + 1] : n;
                    }
                    break;
                }
                case 0: if (n) buf[next() % n] = (uint8_t)next(); break;      /* byte flip */
                case 1: if (n > 36) buf[36 + next() % (n - 36)] = (uint8_t)next(); break;
                case 2: n = next() % (n + 1); break;                          /* truncate */
                case 3: if (n < sizeof buf - 8) { for (int k = 0; k < 8; k++) buf[n++] = (uint8_t)next(); } break;
                }
            }
        }
        /* Copy to an exact-size heap block so ASan sees a read one past the end. */
        uint8_t *exact = malloc(n ? n : 1);
        memcpy(exact, buf, n);
        beacon_parse(exact, n, &bi);
        if (bi.ssid_len > 32) { g_fail++; printf("  FAIL  ssid_len %u > 32\n", bi.ssid_len); free(exact); return; }
        free(exact);
    }
    g_pass++;
    printf("  PASS  fuzz: %ld frames, no over-read (ASan/UBSan)\n", iterations);
}

int main(int argc, char **argv)
{
    long iterations = argc > 1 ? atol(argv[1]) : 200000;
    known_answers();
    fuzz(iterations);
    printf("\n%s: %d passed, %d failed\n", g_fail ? "beacon test FAILED" : "beacon test OK", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
