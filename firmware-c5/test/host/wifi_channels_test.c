/*
 * wifi_channels_test.c — host tests for firmware-c5/main/wifi_channels.c
 * (set_band / set_channels / set_region channel-list filtering, OCP-SPEC §10.9).
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_channels.h"
#include "ocp.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(cond, what) do { if (!(cond)) { printf("FAIL: %s\n", what); failures++; } } while (0)

int main(void)
{
    size_t n = 0;
    const uint8_t *chs = NULL;
    char buf[128];

    /* Defaults: US + auto + all. */
    wifi_channels_reset();
    CHECK(strcmp(wifi_region()->code, "US") == 0, "default region is US");
    CHECK(wifi_band_scope() == WIFI_BAND_SCOPE_AUTO, "default band is auto");
    CHECK(strcmp(wifi_band_scope_name(wifi_band_scope()), OCP_BAND_AUTO) == 0, "band name auto");
    CHECK(!wifi_channels_is_custom(), "default channels not custom");
    chs = wifi_channels(&n);
    CHECK(n == 20 && chs[0] == 1 && chs[10] == 11 && chs[11] == 36 && chs[19] == 165,
          "default US auto list has 20 channels");

    /* Band parser: strict case-sensitive match. */
    wifi_band_scope_t parsed = WIFI_BAND_SCOPE_24;
    CHECK(wifi_band_scope_parse("auto", &parsed) && parsed == WIFI_BAND_SCOPE_AUTO, "parse auto");
    CHECK(wifi_band_scope_parse("24", &parsed) && parsed == WIFI_BAND_SCOPE_24, "parse 24");
    CHECK(wifi_band_scope_parse("5", &parsed) && parsed == WIFI_BAND_SCOPE_5, "parse 5");
    CHECK(!wifi_band_scope_parse("AUTO", &parsed) && parsed == WIFI_BAND_SCOPE_5, "reject AUTO");
    CHECK(!wifi_band_scope_parse("2.4", &parsed), "reject 2.4");
    CHECK(!wifi_band_scope_parse("6", &parsed), "reject 6");
    CHECK(!wifi_band_scope_parse("", &parsed), "reject empty");
    CHECK(!wifi_band_scope_parse(NULL, &parsed), "reject NULL");

    /* Band filtering across regions. */
    wifi_band_select(WIFI_BAND_SCOPE_24);
    chs = wifi_channels(&n);
    CHECK(n == 11 && chs[0] == 1 && chs[10] == 11, "US band=24 has 1..11");
    wifi_channels_format_list(buf, sizeof buf);
    CHECK(strcmp(buf, "1,2,3,4,5,6,7,8,9,10,11") == 0, "US band=24 formatted list");

    wifi_band_select(WIFI_BAND_SCOPE_5);
    chs = wifi_channels(&n);
    CHECK(n == 9 && chs[0] == 36 && chs[8] == 165, "US band=5 has 36..48,149..165");
    wifi_channels_format_list(buf, sizeof buf);
    CHECK(strcmp(buf, "36,40,44,48,149,153,157,161,165") == 0, "US band=5 formatted list");

    /* Region change preserves active band and clears custom list. */
    wifi_region_select(ocp_wifi_region_find("JP"));
    chs = wifi_channels(&n);
    CHECK(wifi_band_scope() == WIFI_BAND_SCOPE_5, "region change keeps band=5");
    CHECK(n == 4 && chs[0] == 36 && chs[3] == 48, "JP band=5 has 36..48 only");

    wifi_band_select(WIFI_BAND_SCOPE_24);
    chs = wifi_channels(&n);
    CHECK(n == 14 && chs[13] == 14, "JP band=24 has 1..14");

    /* Custom channel list validation under US + band=24. */
    wifi_region_select(ocp_wifi_region_find("US"));
    CHECK(wifi_channels_set("1,6,11"), "set_channels 1,6,11 accepted");
    CHECK(wifi_channels_is_custom(), "custom flag set");
    chs = wifi_channels(&n);
    CHECK(n == 3 && chs[0] == 1 && chs[1] == 6 && chs[2] == 11, "custom 1,6,11 active");
    wifi_channels_format_list(buf, sizeof buf);
    CHECK(strcmp(buf, "1,6,11") == 0, "custom list formatted");

    /* Adversarial inputs: every one must fail AND preserve 1,6,11 untouched. */
    const char *bad[] = {
        "", ",", "1,", ",1", "1,,6", " 1", "1 ", "+1", "-1", "0", "256", "9999",
        "abc", "1,6,1",   /* duplicate */
        "12",             /* outside US */
        "52",             /* DFS */
        "36",             /* outside active band=24 */
        "1,36",           /* mixed with out-of-band */
        "ALL",            /* case-sensitive */
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CHECK(!wifi_channels_set(bad[i]), bad[i]);
    }
    CHECK(!wifi_channels_set(NULL), "reject NULL channels");
    chs = wifi_channels(&n);
    CHECK(wifi_channels_is_custom() && n == 3 && chs[0] == 1 && chs[1] == 6 && chs[2] == 11,
          "failed set_channels leaves previous custom list untouched");

    /* Single channel and "all" reset. */
    CHECK(wifi_channels_set("6"), "single channel 6 accepted");
    chs = wifi_channels(&n);
    CHECK(n == 1 && chs[0] == 6, "single channel active");
    CHECK(wifi_channels_set("all"), "all accepted");
    CHECK(!wifi_channels_is_custom(), "all clears custom flag");
    chs = wifi_channels(&n);
    CHECK(n == 11 && chs[0] == 1 && chs[10] == 11, "all restores US band=24");

    /* Band=5 rejects 2.4 GHz custom channels and accepts 5 GHz ones. */
    wifi_band_select(WIFI_BAND_SCOPE_5);
    CHECK(!wifi_channels_set("6"), "band=5 rejects ch 6");
    CHECK(wifi_channels_set("36,149"), "band=5 accepts 36,149");
    chs = wifi_channels(&n);
    CHECK(n == 2 && chs[0] == 36 && chs[1] == 149, "band=5 custom 36,149 active");

    /* Switching band clears custom list. */
    wifi_band_select(WIFI_BAND_SCOPE_AUTO);
    CHECK(!wifi_channels_is_custom(), "band switch clears custom flag");
    CHECK(wifi_channels_set("1,36,165"), "band=auto allows mixed 2.4+5");
    chs = wifi_channels(&n);
    CHECK(n == 3 && chs[0] == 1 && chs[1] == 36 && chs[2] == 165, "mixed list active");

    /* Region change clears custom list. */
    wifi_region_select(ocp_wifi_region_find("EU"));
    CHECK(!wifi_channels_is_custom(), "region switch clears custom flag");
    chs = wifi_channels(&n);
    CHECK(n == 22 && chs[12] == 13, "EU auto has 22 channels incl. ch 13");

    /* Truncation safety on format buffer. */
    wifi_channels_format_list(buf, 5);
    CHECK(strcmp(buf, "1,2") == 0, "format drops entry that does not fit");

    if (failures) { printf("wifi channels: %d failure(s)\n", failures); return 1; }
    printf("wifi channels: band/channel filtering, validation and rollback passed\n");
    return 0;
}
