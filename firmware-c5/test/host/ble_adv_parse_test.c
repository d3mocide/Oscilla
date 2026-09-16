/*
 * ble_adv_parse_test.c — known-answer tests and a mutation fuzzer for
 * ble_adv_parse. Build with -fsanitize=address,undefined: any out-of-bounds
 * read aborts. Same shape as beacon_test.c.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ble_adv_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, what) do { \
    if (cond) g_pass++; else { g_fail++; printf("  FAIL  %s (line %d)\n", what, __LINE__); } \
} while (0)

typedef struct { uint8_t b[64]; size_t n; } adv_t;

static void ad(adv_t *f, uint8_t type, const uint8_t *data, uint8_t data_len)
{
    f->b[f->n++] = (uint8_t)(data_len + 1);
    f->b[f->n++] = type;
    memcpy(f->b + f->n, data, data_len);
    f->n += data_len;
}

/* Apple Find My (AirTag) manufacturer payload: type 0x12, then whatever
 * status/key bytes — classification only looks at the type byte. */
static void ad_airtag(adv_t *f)
{
    uint8_t payload[] = { 0x4c, 0x00, 0x12, 0x19, 0x00, 0xaa, 0xbb };
    ad(f, 0xFF, payload, sizeof payload);
}

/* Apple iBeacon: same company ID, different type byte (0x02) — must NOT
 * classify as a tracker. The company ID alone is not the signature. */
static void ad_ibeacon(adv_t *f)
{
    uint8_t payload[] = { 0x4c, 0x00, 0x02, 0x15, 0x01, 0x02, 0x03 };
    ad(f, 0xFF, payload, sizeof payload);
}

static uint32_t rng = 0x9E3779B9u;
static uint32_t next(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static void known_answers(void)
{
    ble_adv_info_t info;

    {
        ble_adv_parse(NULL, 0, &info);
        CHECK(!info.has_name && !info.has_mfr && !info.is_tracker, "empty payload parses to all-defaults");
    }
    {
        adv_t f = {0};
        uint8_t name[] = "Pixel Buds";
        ad(&f, 0x09, name, (uint8_t)strlen((char *)name));
        ble_adv_parse(f.b, f.n, &info);
        CHECK(info.has_name && info.name_len == strlen((char *)name) &&
              memcmp(info.name, name, info.name_len) == 0, "complete local name parses");
        CHECK(!info.has_mfr && !info.is_tracker, "no manufacturer data present");
    }
    {
        /* Complete name (0x09) wins even when the shortened one (0x08) comes first. */
        adv_t f = {0};
        uint8_t shortn[] = "Short";
        uint8_t full[] = "CompleteName";
        ad(&f, 0x08, shortn, (uint8_t)strlen((char *)shortn));
        ad(&f, 0x09, full, (uint8_t)strlen((char *)full));
        ble_adv_parse(f.b, f.n, &info);
        CHECK(info.name_len == strlen((char *)full) && memcmp(info.name, full, info.name_len) == 0,
              "complete name (0x09) wins over shortened (0x08)");
    }
    {
        adv_t f = {0};
        ad_airtag(&f);
        ble_adv_parse(f.b, f.n, &info);
        CHECK(info.has_mfr && info.mfr_company_id == 0x004C, "Apple company ID parsed little-endian");
        CHECK(info.is_tracker, "Find My type byte 0x12 classifies as a tracker");
    }
    {
        adv_t f = {0};
        ad_ibeacon(&f);
        ble_adv_parse(f.b, f.n, &info);
        CHECK(info.has_mfr && info.mfr_company_id == 0x004C, "iBeacon also carries Apple's company ID");
        CHECK(!info.is_tracker, "iBeacon (type 0x02) is not misclassified as a tracker");
    }
    {
        /* Manufacturer data with fewer than 2 bytes (no room for a company ID) is ignored. */
        adv_t f = {0};
        uint8_t one[] = { 0x4c };
        ad(&f, 0xFF, one, sizeof one);
        ble_adv_parse(f.b, f.n, &info);
        CHECK(!info.has_mfr, "manufacturer data too short for a company ID is dropped, not misread");
    }
    {
        /* A length byte that claims more than the buffer holds is dropped, not over-read. */
        adv_t f = {0};
        f.b[f.n++] = 0x10;   /* claims 16 more bytes */
        f.b[f.n++] = 0x09;
        f.b[f.n++] = 'X';
        ble_adv_parse(f.b, f.n, &info);
        CHECK(!info.has_name, "an overlong AD structure is dropped, not read past the buffer");
    }
    {
        /* A zero length byte is padding: stop, don't loop forever or misread what follows. */
        adv_t f = {0};
        f.b[f.n++] = 0x00;
        uint8_t name[] = "After";
        ad(&f, 0x09, name, (uint8_t)strlen((char *)name));
        ble_adv_parse(f.b, f.n, &info);
        CHECK(!info.has_name, "zero-length AD structure ends the walk (it's padding)");
    }
    {
        /* First manufacturer-data block wins if more than one is present. */
        adv_t f = {0};
        ad_airtag(&f);
        ad_ibeacon(&f);
        ble_adv_parse(f.b, f.n, &info);
        CHECK(info.is_tracker, "first manufacturer-data block wins (airtag first)");
    }
}

static void fuzz(long iterations)
{
    ble_adv_info_t info;
    adv_t seed = {0};
    ad_airtag(&seed);
    uint8_t name[] = "Fuzz";
    ad(&seed, 0x09, name, sizeof name - 1);

    for (long i = 0; i < iterations; i++) {
        uint8_t buf[128];
        size_t n;
        if (i % 3 == 0) {
            n = next() % sizeof buf;
            for (size_t k = 0; k < n; k++) buf[k] = (uint8_t)next();
        } else {
            n = seed.n;
            memcpy(buf, seed.b, n);
            int muts = 1 + next() % 6;
            for (int m = 0; m < muts; m++) {
                switch (next() % 4) {
                case 0: if (n) buf[next() % n] = (uint8_t)next(); break;       /* byte flip */
                case 1: n = next() % (n + 1); break;                          /* truncate */
                case 2: if (n < sizeof buf - 4) { for (int k = 0; k < 4; k++) buf[n++] = (uint8_t)next(); } break;
                case 3: if (n) buf[0] = (uint8_t)next(); break;               /* mangle the length byte */
                }
            }
        }
        uint8_t *exact = malloc(n ? n : 1);
        memcpy(exact, buf, n);
        ble_adv_parse(exact, n, &info);
        if (info.name_len > sizeof info.name || info.mfr_data_len > sizeof info.mfr_data) {
            g_fail++;
            printf("  FAIL  parsed field longer than its own buffer\n");
            free(exact);
            return;
        }
        free(exact);
    }
    g_pass++;
    printf("  PASS  fuzz: %ld payloads, no over-read (ASan/UBSan)\n", iterations);
}

int main(int argc, char **argv)
{
    long iterations = argc > 1 ? atol(argv[1]) : 200000;
    known_answers();
    fuzz(iterations);
    printf("\n%s: %d passed, %d failed\n", g_fail ? "ble adv parse test FAILED" : "ble adv parse test OK",
           g_pass, g_fail);
    return g_fail ? 1 : 0;
}
