/*
 * wifi_network_table_test.c — bounded AP/BSSID dedup tests.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_network_table.h"

#include <stdio.h>
#include <string.h>

static int pass, fail;
#define CHECK(c, s) do { if (c) pass++; else { fail++; printf("  FAIL %s\n", s); } } while (0)

static beacon_info_t info(uint8_t last, const char *ssid, bool rsn)
{
    beacon_info_t b;
    memset(&b, 0, sizeof b);
    b.bssid[0] = 0x02; b.bssid[5] = last;
    b.has_ssid = true;
    b.ssid_len = (uint8_t)strlen(ssid);
    memcpy(b.ssid, ssid, b.ssid_len);
    b.has_rsn = rsn;
    b.mfp_capable = rsn;
    b.interval_tu = 100;
    return b;
}

int main(void)
{
    wifi_network_table_t t;
    wifi_network_t out;
    wifi_network_table_reset(&t);
    beacon_info_t b = info(1, "HomeNet", true);
    CHECK(wifi_network_table_upsert(&t, &b, 6, -50, &out) && t.count == 1, "first BSSID inserts");
    CHECK(out.ssid_len == 7 && out.channel == 6 && out.has_rsn && out.mfp_capable && out.seen == 1,
          "first row fields copied");

    b = info(1, "HomeNet", true);
    CHECK(!wifi_network_table_upsert(&t, &b, 36, -40, &out) && t.count == 1,
          "repeat BSSID updates in place");
    CHECK(t.networks[0].channel == 36 && t.networks[0].rssi == -40 && t.networks[0].seen == 2,
          "repeat takes latest channel and RSSI");

    memset(&b, 0, sizeof b);
    b.bssid[0] = 0x02; b.bssid[5] = 2;
    b.interval_tu = 100;
    CHECK(wifi_network_table_upsert(&t, &b, 11, -70, &out) && !out.has_ssid && out.ssid_len == 0,
          "hidden BSSID inserts without an SSID");

    wifi_network_table_reset(&t);
    for (unsigned i = 0; i < WIFI_NETWORKS_MAX; i++) {
        b = info((uint8_t)i, "x", false);
        b.bssid[4] = (uint8_t)(i >> 8);
        CHECK(wifi_network_table_upsert(&t, &b, 1, -80, NULL), "table fills");
    }
    b = info(0xfe, "overflow", false);
    b.bssid[4] = 0xaa;
    CHECK(t.count == WIFI_NETWORKS_MAX && !wifi_network_table_upsert(&t, &b, 1, -80, NULL),
          "full table drops new BSSID");

    printf("%s: %d passed, %d failed\n", fail ? "wifi network table FAILED" : "wifi network table OK", pass, fail);
    return fail != 0;
}
