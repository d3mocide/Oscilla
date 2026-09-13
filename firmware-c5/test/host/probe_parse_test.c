/*
 * probe_parse_test.c — known-answer tests and a mutation fuzzer for
 * probe_parse. Build with -fsanitize=address,undefined: any out-of-bounds
 * read aborts.
 *
 * SPDX-License-Identifier: MIT
 */

#include "probe_parse.h"

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

/* Probe request header: no fixed fields before the IEs (unlike a beacon). */
static void header(frame_t *f, const uint8_t *mac)
{
    memset(f, 0, sizeof *f);
    put8(f, 0x40); put8(f, 0);                       /* frame control: probe req */
    put16(f, 0);                                     /* duration */
    for (int i = 0; i < 6; i++) put8(f, 0xff);       /* addr1: DA (broadcast) */
    put(f, mac, 6);                                   /* addr2: SA (transmitter) */
    for (int i = 0; i < 6; i++) put8(f, 0xff);       /* addr3: BSSID (wildcard) */
    put16(f, 0);                                      /* seq */
}

static void ie(frame_t *f, uint8_t id, const uint8_t *p, uint8_t n) { put8(f, id); put8(f, n); put(f, p, n); }

/* Parse from an exact-size heap copy, so ASan flags even a 1-byte over-read. */
static probe_result_t parse_exact(const uint8_t *b, size_t n, probe_req_info_t *pi)
{
    uint8_t *copy = malloc(n ? n : 1);
    memcpy(copy, b, n);
    probe_result_t r = probe_req_parse(copy, n, pi);
    free(copy);
    return r;
}

static const uint8_t k_mac[6] = { 0xf4, 0x12, 0x34, 0x56, 0x78, 0x9a };

static void known_answers(void)
{
    frame_t f;
    probe_req_info_t pi;

    header(&f, k_mac);
    uint8_t ssid[] = "HomeNet";
    ie(&f, 0, ssid, 7);
    CHECK(parse_exact(f.b, f.n, &pi) == PROBE_OK, "valid probe request parses");
    CHECK(memcmp(pi.mac, k_mac, 6) == 0, "transmitter MAC from addr2");
    CHECK(pi.has_ssid && pi.ssid_len == 7 && memcmp(pi.ssid, "HomeNet", 7) == 0, "directed SSID");

    header(&f, k_mac);
    ie(&f, 0, ssid, 0);                               /* wildcard: zero-length SSID IE */
    CHECK(parse_exact(f.b, f.n, &pi) == PROBE_OK && pi.has_ssid && pi.ssid_len == 0,
          "wildcard probe: SSID element present but empty");

    header(&f, k_mac);                                /* no SSID IE at all */
    CHECK(parse_exact(f.b, f.n, &pi) == PROBE_OK && !pi.has_ssid, "no SSID element at all");

    header(&f, k_mac);
    uint8_t big[64]; memset(big, 'S', sizeof big);
    ie(&f, 0, big, sizeof big);
    parse_exact(f.b, f.n, &pi);
    CHECK(pi.has_ssid && pi.ssid_len == 32, "over-long SSID clamped to 32");

    header(&f, k_mac);
    uint8_t rates[] = { 0x82, 0x84 };
    ie(&f, 1, rates, sizeof rates);                   /* supported rates before SSID */
    ie(&f, 0, ssid, 7);
    parse_exact(f.b, f.n, &pi);
    CHECK(pi.has_ssid && pi.ssid_len == 7, "SSID found after a preceding element");

    header(&f, k_mac);
    put8(&f, 0); put8(&f, 200); put8(&f, 'x');        /* SSID IE claims 200 bytes */
    CHECK(parse_exact(f.b, f.n, &pi) == PROBE_OK && pi.ies_truncated && !pi.has_ssid,
          "element running past the frame: truncated, not read");

    header(&f, k_mac);
    put8(&f, 0);                                      /* length byte missing entirely */
    CHECK(parse_exact(f.b, f.n, &pi) == PROBE_OK && pi.ies_truncated,
          "a lone element-id byte with no length: truncated");

    /* Non-probe-request frame types are rejected outright. */
    header(&f, k_mac); f.b[0] = 0x80;                 /* beacon */
    CHECK(parse_exact(f.b, f.n, &pi) == PROBE_NOT_REQUEST, "beacon rejected");
    header(&f, k_mac); f.b[0] = 0x50;                 /* probe response */
    CHECK(parse_exact(f.b, f.n, &pi) == PROBE_NOT_REQUEST, "probe response rejected");
    header(&f, k_mac); f.b[0] = 0x08;                 /* data frame */
    CHECK(parse_exact(f.b, f.n, &pi) == PROBE_NOT_REQUEST, "data frame rejected");

    header(&f, k_mac);
    CHECK(parse_exact(f.b, 23, &pi) == PROBE_TOO_SHORT, "23 bytes: too short");
    CHECK(parse_exact(f.b, 0, &pi) == PROBE_TOO_SHORT, "empty: too short");
    CHECK(parse_exact(f.b, 24, &pi) == PROBE_OK && !pi.has_ssid, "exactly the header: valid, no IEs");
}

static uint32_t rng = 0x9A785634u;
static uint32_t next(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static void fuzz(long iterations)
{
    probe_req_info_t pi;
    frame_t seed;
    header(&seed, k_mac);
    uint8_t ssid[] = "Fuzz"; ie(&seed, 0, ssid, 4);

    for (long i = 0; i < iterations; i++) {
        uint8_t buf[512];
        size_t n;
        if (i % 3 == 0) {                            /* pure random */
            n = next() % sizeof buf;
            for (size_t k = 0; k < n; k++) buf[k] = (uint8_t)next();
            if (n) buf[0] = 0x40;                     /* keep it plausibly a probe request */
        } else {                                      /* mutate a valid probe request */
            n = seed.n;
            memcpy(buf, seed.b, n);
            int muts = 1 + next() % 6;
            for (int m = 0; m < muts; m++) {
                switch (next() % 4) {
                case 0: if (n) buf[next() % n] = (uint8_t)next(); break;       /* byte flip */
                case 1: if (n > 24) buf[24 + next() % (n - 24)] = (uint8_t)next(); break;
                case 2: n = next() % (n + 1); break;                           /* truncate */
                case 3: if (n < sizeof buf - 8) { for (int k = 0; k < 8; k++) buf[n++] = (uint8_t)next(); } break;
                }
            }
        }
        uint8_t *exact = malloc(n ? n : 1);
        memcpy(exact, buf, n);
        probe_req_parse(exact, n, &pi);
        if (pi.ssid_len > 32) { g_fail++; printf("  FAIL  ssid_len %u > 32\n", pi.ssid_len); free(exact); return; }
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
    printf("\n%s: %d passed, %d failed\n", g_fail ? "probe_parse test FAILED" : "probe_parse test OK", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
