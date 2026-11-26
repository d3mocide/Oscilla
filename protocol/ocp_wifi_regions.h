/*
 * ocp_wifi_regions.h — the Wi-Fi regions `set_region` accepts (OCP-SPEC
 * §10.8). One table for both firmwares: the probe hops these channels and
 * sets the matching ESP-IDF country; the deck offers them in Settings.
 *
 * Each list is the non-DFS channels that ESP-IDF's regulatory database
 * (components/esp_wifi/regulatory/esp_wifi_regulatory.txt) allows for that
 * country, checked against IDF 5.5.6. DFS channels stay out of every list
 * (D-14). CA gained 2400-2483.5 MHz (ch 12-13) in 5.5.6's database; its
 * 5850-5895 MHz (ch 169-177) is not listed until the C5 is shown to receive
 * there.
 * Receive-only (D-8): a region decides which channels are listened to and
 * never enables transmission.
 *
 * Pure C, header-only (static inline), so host tests and both firmwares
 * include it unchanged.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_OCP_WIFI_REGIONS_H
#define OSCILLA_OCP_WIFI_REGIONS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define OCP_WIFI_REGION_DEFAULT      "US"
#define OCP_WIFI_REGION_MAX_CHANNELS 24

typedef struct {
    const char *code;      /* wire value of `set_region` and `region=` */
    const char *country;   /* esp_wifi_set_country_code() argument */
    const char *name;      /* deck display */
    uint8_t count;
    uint8_t channels[OCP_WIFI_REGION_MAX_CHANNELS];
} ocp_wifi_region_t;

#define OCP_WIFI_24_1_11   1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
#define OCP_WIFI_24_1_13   OCP_WIFI_24_1_11, 12, 13
#define OCP_WIFI_5_LOW     36, 40, 44, 48                   /* UNII-1 */
#define OCP_WIFI_5_HIGH    149, 153, 157, 161, 165          /* UNII-3 */

static inline const ocp_wifi_region_t *ocp_wifi_region_table(size_t *count)
{
    static const ocp_wifi_region_t k_regions[] = {
        /* 2400-2472 MHz: ch 1-11. 5150-5250, 5730-5855 MHz. */
        { "US", "US", "UNITED STATES", 20, { OCP_WIFI_24_1_11, OCP_WIFI_5_LOW, OCP_WIFI_5_HIGH } },
        /* 2400-2483.5 MHz: ch 1-13 (IDF 5.5.6). 5150-5250, 5730-5850 MHz. */
        { "CA", "CA", "CANADA",        22, { OCP_WIFI_24_1_13, OCP_WIFI_5_LOW, OCP_WIFI_5_HIGH } },
        /* 2402-2482 MHz: ch 1-13. 5170-5250, 5735-5835 MHz. */
        { "MX", "MX", "MEXICO",        22, { OCP_WIFI_24_1_13, OCP_WIFI_5_LOW, OCP_WIFI_5_HIGH } },
        /* CEPT (country EU/DE rows): 2400-2483.5 MHz, 5150-5250, SRD 5725-5875 MHz. */
        { "EU", "DE", "EUROPE (CEPT)", 22, { OCP_WIFI_24_1_13, OCP_WIFI_5_LOW, OCP_WIFI_5_HIGH } },
        /* 2400-2483.5 MHz, 5150-5250, 5725-5850 MHz. */
        { "GB", "GB", "UNITED KINGDOM", 22, { OCP_WIFI_24_1_13, OCP_WIFI_5_LOW, OCP_WIFI_5_HIGH } },
        /* 2402-2482 MHz plus ch 14 (2474-2494, NO-OFDM). 5170-5250 MHz only. */
        { "JP", "JP", "JAPAN",         18, { OCP_WIFI_24_1_13, 14, OCP_WIFI_5_LOW } },
    };
    *count = sizeof k_regions / sizeof k_regions[0];
    return k_regions;
}

/* NULL for anything not in the table (the wire value is case-sensitive). */
static inline const ocp_wifi_region_t *ocp_wifi_region_find(const char *code)
{
    size_t n;
    const ocp_wifi_region_t *t = ocp_wifi_region_table(&n);
    for (size_t i = 0; code && i < n; i++)
        if (strcmp(t[i].code, code) == 0) return &t[i];
    return NULL;
}

/* "1-11,36-48,149-165": runs of 2.4 GHz channels step 1, 5 GHz step 4.
 * Always NUL-terminated; a run that would not fit is left off whole. */
static inline void ocp_wifi_region_summary(const ocp_wifi_region_t *r, char *out, size_t n)
{
    size_t len = 0;
    if (!n) return;
    out[0] = '\0';
    for (size_t i = 0; i < r->count;) {
        const unsigned step = r->channels[i] <= 14 ? 1u : 4u;
        size_t j = i;
        while (j + 1 < r->count && r->channels[j + 1] == r->channels[j] + step) j++;
        int w = j > i ? snprintf(out + len, n - len, "%s%u-%u", len ? "," : "",
                                 (unsigned)r->channels[i], (unsigned)r->channels[j])
                      : snprintf(out + len, n - len, "%s%u", len ? "," : "",
                                 (unsigned)r->channels[i]);
        if (w < 0 || (size_t)w >= n - len) { out[len] = '\0'; return; }
        len += (size_t)w;
        i = j + 1;
    }
}

/* "1,2,...,165": the exact hop list, for the `chans=` reply key. */
static inline void ocp_wifi_region_list(const ocp_wifi_region_t *r, char *out, size_t n)
{
    size_t len = 0;
    if (!n) return;
    out[0] = '\0';
    for (size_t i = 0; i < r->count; i++) {
        int w = snprintf(out + len, n - len, "%s%u", i ? "," : "", (unsigned)r->channels[i]);
        if (w < 0 || (size_t)w >= n - len) { out[len] = '\0'; return; }
        len += (size_t)w;
    }
}

#endif /* OSCILLA_OCP_WIFI_REGIONS_H */
