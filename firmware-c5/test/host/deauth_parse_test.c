/*
 * deauth_parse_test.c — known-answer tests and a mutation fuzzer for
 * deauth_parse. Build with -fsanitize=address,undefined: any out-of-bounds
 * read aborts.
 *
 * SPDX-License-Identifier: MIT
 */

#include "deauth_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, what) do { \
    if (cond) g_pass++; else { g_fail++; printf("  FAIL  %s (line %d)\n", what, __LINE__); } \
} while (0)

typedef struct { uint8_t b[32]; size_t n; } frame_t;

static void put(frame_t *f, const uint8_t *p, size_t n) { memcpy(f->b + f->n, p, n); f->n += n; }
static void put8(frame_t *f, uint8_t v) { f->b[f->n++] = v; }
static void put16(frame_t *f, uint16_t v) { put8(f, v & 0xFF); put8(f, v >> 8); }

static const uint8_t k_dest[6] = { 0xf4, 0x12, 0x34, 0x56, 0x78, 0x9a };
static const uint8_t k_bssid[6] = { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01 };
static const uint8_t k_bcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static void header(frame_t *f, uint8_t fc, const uint8_t *dest, uint16_t reason)
{
    memset(f, 0, sizeof *f);
    put8(f, fc); put8(f, 0);          /* frame control */
    put16(f, 0);                       /* duration */
    put(f, dest, 6);                   /* addr1 */
    put(f, k_bssid, 6);                /* addr2 (transmitter, normally == BSSID) */
    put(f, k_bssid, 6);                /* addr3 (BSSID) */
    put16(f, 0);                       /* seq */
    put16(f, reason);                  /* reason code: the entire frame body */
}

static deauth_result_t parse_exact(const uint8_t *b, size_t n, deauth_info_t *di)
{
    uint8_t *copy = malloc(n ? n : 1);
    memcpy(copy, b, n);
    deauth_result_t r = deauth_parse(copy, n, di);
    free(copy);
    return r;
}

static void known_answers(void)
{
    frame_t f;
    deauth_info_t di;

    header(&f, 0xC0, k_dest, 7);   /* deauth, reason 7: class 3 frame from nonassoc STA */
    CHECK(parse_exact(f.b, f.n, &di) == DEAUTH_OK, "valid deauth parses");
    CHECK(!di.is_disassoc, "subtype 0xC0 is deauth, not disassoc");
    CHECK(memcmp(di.dest, k_dest, 6) == 0, "dest from addr1");
    CHECK(memcmp(di.bssid, k_bssid, 6) == 0, "bssid from addr3");
    CHECK(di.reason == 7, "reason code little-endian");

    header(&f, 0xA0, k_dest, 8);   /* disassoc */
    CHECK(parse_exact(f.b, f.n, &di) == DEAUTH_OK && di.is_disassoc, "subtype 0xA0 is disassoc");

    header(&f, 0xC0, k_bcast, 2);  /* broadcast deauth: whole-network kick */
    CHECK(parse_exact(f.b, f.n, &di) == DEAUTH_OK, "broadcast destination is a valid frame");
    CHECK(memcmp(di.dest, k_bcast, 6) == 0, "broadcast dest preserved, not filtered here");

    header(&f, 0x80, k_dest, 0);   /* beacon */
    CHECK(parse_exact(f.b, f.n, &di) == DEAUTH_NOT_DEAUTH, "beacon rejected");
    header(&f, 0x40, k_dest, 0);   /* probe request */
    CHECK(parse_exact(f.b, f.n, &di) == DEAUTH_NOT_DEAUTH, "probe request rejected");
    header(&f, 0x08, k_dest, 0);   /* data frame */
    CHECK(parse_exact(f.b, f.n, &di) == DEAUTH_NOT_DEAUTH, "data frame rejected");

    header(&f, 0xC0, k_dest, 0);
    CHECK(parse_exact(f.b, 25, &di) == DEAUTH_TOO_SHORT, "25 bytes: reason code truncated");
    CHECK(parse_exact(f.b, 0, &di) == DEAUTH_TOO_SHORT, "empty: too short");
    CHECK(parse_exact(f.b, 26, &di) == DEAUTH_OK, "exactly header + reason code: valid");

    /* A trailing byte (a malformed/extended frame) doesn't confuse the parser:
     * this frame type has no IEs to (mis)read past the fixed body. */
    header(&f, 0xC0, k_dest, 3);
    put8(&f, 0xFF);
    CHECK(parse_exact(f.b, f.n, &di) == DEAUTH_OK && di.reason == 3, "trailing byte ignored, reason still correct");
}

static uint32_t rng = 0xDEA47Au;
static uint32_t next(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static void fuzz(long iterations)
{
    deauth_info_t di;
    frame_t seed;
    header(&seed, 0xC0, k_dest, 7);

    for (long i = 0; i < iterations; i++) {
        uint8_t buf[64];
        size_t n;
        if (i % 3 == 0) {
            n = next() % sizeof buf;
            for (size_t k = 0; k < n; k++) buf[k] = (uint8_t)next();
            if (n) buf[0] = (next() & 1) ? 0xC0 : 0xA0;
        } else {
            n = seed.n;
            memcpy(buf, seed.b, n);
            int muts = 1 + next() % 4;
            for (int m = 0; m < muts; m++) {
                switch (next() % 3) {
                case 0: if (n) buf[next() % n] = (uint8_t)next(); break;
                case 1: n = next() % (n + 1); break;                         /* truncate */
                case 2: if (n < sizeof buf - 4) { for (int k = 0; k < 4; k++) buf[n++] = (uint8_t)next(); } break;
                }
            }
        }
        uint8_t *exact = malloc(n ? n : 1);
        memcpy(exact, buf, n);
        deauth_parse(exact, n, &di);
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
    printf("\n%s: %d passed, %d failed\n", g_fail ? "deauth_parse test FAILED" : "deauth_parse test OK", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
