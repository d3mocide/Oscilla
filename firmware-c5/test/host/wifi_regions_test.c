/*
 * wifi_regions_test.c — protocol/ocp_wifi_regions.h: every region's hop list
 * is well-formed, DFS-free and matches the regulatory rows it cites.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_wifi_regions.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond, what) do { if (!(cond)) { printf("FAIL: %s\n", what); failures++; } } while (0)

static int has(const ocp_wifi_region_t *r, unsigned ch)
{
    for (size_t i = 0; i < r->count; i++) if (r->channels[i] == ch) return 1;
    return 0;
}

int main(void)
{
    size_t n;
    const ocp_wifi_region_t *t = ocp_wifi_region_table(&n);
    CHECK(n >= 1, "table is not empty");

    for (size_t i = 0; i < n; i++) {
        const ocp_wifi_region_t *r = &t[i];
        char what[96];
        snprintf(what, sizeof what, "%s: count matches the listed channels", r->code);
        size_t listed = 0;
        while (listed < OCP_WIFI_REGION_MAX_CHANNELS && r->channels[listed]) listed++;
        CHECK(listed == r->count, what);
        snprintf(what, sizeof what, "%s: channels strictly increase", r->code);
        for (size_t k = 1; k < r->count; k++) CHECK(r->channels[k] > r->channels[k - 1], what);
        snprintf(what, sizeof what, "%s: no DFS channel (52-144, D-14)", r->code);
        for (size_t k = 0; k < r->count; k++) CHECK(r->channels[k] < 52 || r->channels[k] > 144, what);
        snprintf(what, sizeof what, "%s: every channel is a real 2.4/5 GHz channel", r->code);
        for (size_t k = 0; k < r->count; k++) {
            unsigned c = r->channels[k];
            CHECK((c >= 1 && c <= 14) || (c >= 36 && c <= 165 && c % 4 == (c >= 149 ? 1u : 0u)), what);
        }
        snprintf(what, sizeof what, "%s: ch 14 only in JP", r->code);
        CHECK(!has(r, 14) || strcmp(r->code, "JP") == 0, what);
        snprintf(what, sizeof what, "%s: code is two capitals (bare OCP value)", r->code);
        CHECK(strlen(r->code) == 2 && r->code[0] >= 'A' && r->code[0] <= 'Z' &&
              r->code[1] >= 'A' && r->code[1] <= 'Z', what);
        for (size_t j = 0; j < i; j++) {
            snprintf(what, sizeof what, "%s: code is unique", r->code);
            CHECK(strcmp(t[j].code, r->code) != 0, what);
        }
        CHECK(ocp_wifi_region_find(r->code) == r, "find returns the table row");
    }

    const ocp_wifi_region_t *us = ocp_wifi_region_find(OCP_WIFI_REGION_DEFAULT);
    CHECK(us && strcmp(us->code, "US") == 0, "default region is US");
    CHECK(us && !has(us, 12) && !has(us, 13), "US stops at ch 11 (FCC 2400-2472 MHz)");
    CHECK(ocp_wifi_region_find("us") == NULL, "codes are case-sensitive");
    CHECK(ocp_wifi_region_find("XX") == NULL, "unknown code is rejected");
    CHECK(ocp_wifi_region_find("") == NULL, "empty code is rejected");
    CHECK(ocp_wifi_region_find(NULL) == NULL, "NULL code is rejected");
    const ocp_wifi_region_t *eu = ocp_wifi_region_find("EU");
    CHECK(eu && has(eu, 12) && has(eu, 13) && !has(eu, 14), "EU hops 1-13");
    CHECK(eu && strcmp(eu->country, "DE") == 0, "EU applies a CEPT country ESP-IDF accepts");

    char buf[96];
    if (us) {
        ocp_wifi_region_summary(us, buf, sizeof buf);
        CHECK(strcmp(buf, "1-11,36-48,149-165") == 0, "US summary");
        ocp_wifi_region_list(us, buf, sizeof buf);
        CHECK(strcmp(buf, "1,2,3,4,5,6,7,8,9,10,11,36,40,44,48,149,153,157,161,165") == 0, "US list");
        ocp_wifi_region_summary(us, buf, 6);
        CHECK(strcmp(buf, "1-11") == 0, "summary drops a run that does not fit");
        ocp_wifi_region_list(us, buf, 4);
        CHECK(strcmp(buf, "1,2") == 0, "list drops a channel that does not fit");
    }
    const ocp_wifi_region_t *jp = ocp_wifi_region_find("JP");
    if (jp) {
        ocp_wifi_region_summary(jp, buf, sizeof buf);
        CHECK(strcmp(buf, "1-14,36-48") == 0, "JP summary");
    }

    if (failures) { printf("wifi regions: %d failure(s)\n", failures); return 1; }
    printf("wifi regions: hop lists well-formed, DFS-free, US 1-11, EU 1-13, JP 1-14 passed\n");
    return 0;
}
