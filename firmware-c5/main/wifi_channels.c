/*
 * wifi_channels.c — see wifi_channels.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_channels.h"

#include <stdio.h>
#include <string.h>

#include "ocp.h"

/* Written only by set_region/set_band/set_channels while no PHY tool runs
 * (ocp_server.c), read by an engine's start; no lock required. */
static const ocp_wifi_region_t *s_region;
static wifi_band_scope_t s_band = WIFI_BAND_SCOPE_AUTO;
static bool s_custom;
static uint8_t s_active[OCP_WIFI_REGION_MAX_CHANNELS];
static size_t s_active_count;

static bool channel_in_band(uint8_t ch, wifi_band_scope_t scope)
{
    if (scope == WIFI_BAND_SCOPE_24) return ch <= 14;
    if (scope == WIFI_BAND_SCOPE_5)  return ch > 14;
    return true;
}

static bool channel_in_region(const ocp_wifi_region_t *r, uint8_t ch)
{
    for (size_t i = 0; i < r->count; i++) {
        if (r->channels[i] == ch) return true;
    }
    return false;
}

static void rebuild_default_list(void)
{
    s_active_count = 0;
    for (size_t i = 0; i < s_region->count; i++) {
        uint8_t ch = s_region->channels[i];
        if (channel_in_band(ch, s_band)) {
            s_active[s_active_count++] = ch;
        }
    }
}

static void ensure_init(void)
{
    if (!s_region) {
        s_region = ocp_wifi_region_find(OCP_WIFI_REGION_DEFAULT);
        s_band = WIFI_BAND_SCOPE_AUTO;
        s_custom = false;
        rebuild_default_list();
    }
}

const ocp_wifi_region_t *wifi_region(void)
{
    ensure_init();
    return s_region;
}

void wifi_region_select(const ocp_wifi_region_t *region)
{
    ensure_init();
    if (!region) return;
    s_region = region;
    s_custom = false;
    rebuild_default_list();
}

wifi_band_scope_t wifi_band_scope(void)
{
    ensure_init();
    return s_band;
}

const char *wifi_band_scope_name(wifi_band_scope_t scope)
{
    switch (scope) {
        case WIFI_BAND_SCOPE_24:   return OCP_BAND_24;
        case WIFI_BAND_SCOPE_5:    return OCP_BAND_5;
        case WIFI_BAND_SCOPE_AUTO: break;
    }
    return OCP_BAND_AUTO;
}

bool wifi_band_scope_parse(const char *s, wifi_band_scope_t *out)
{
    if (!s || !out) return false;
    if (strcmp(s, OCP_BAND_AUTO) == 0) { *out = WIFI_BAND_SCOPE_AUTO; return true; }
    if (strcmp(s, OCP_BAND_24) == 0)   { *out = WIFI_BAND_SCOPE_24;   return true; }
    if (strcmp(s, OCP_BAND_5) == 0)    { *out = WIFI_BAND_SCOPE_5;    return true; }
    return false;
}

void wifi_band_select(wifi_band_scope_t scope)
{
    ensure_init();
    s_band = scope;
    s_custom = false;
    rebuild_default_list();
}

static bool parse_channel_token(const char *start, size_t len, uint8_t *out)
{
    if (len == 0 || len > 3) return false;
    unsigned v = 0;
    for (size_t i = 0; i < len; i++) {
        char c = start[i];
        if (c < '0' || c > '9') return false;
        v = v * 10u + (unsigned)(c - '0');
    }
    if (v == 0 || v > 255u) return false;
    *out = (uint8_t)v;
    return true;
}

bool wifi_channels_set(const char *arg)
{
    ensure_init();
    if (!arg || arg[0] == '\0') return false;

    if (strcmp(arg, OCP_CHANNELS_ALL) == 0) {
        s_custom = false;
        rebuild_default_list();
        return true;
    }

    uint8_t parsed[OCP_WIFI_REGION_MAX_CHANNELS];
    size_t count = 0;
    const char *p = arg;

    for (;;) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        uint8_t ch = 0;
        if (!parse_channel_token(p, len, &ch)) return false;
        if (!channel_in_region(s_region, ch) || !channel_in_band(ch, s_band)) return false;
        for (size_t i = 0; i < count; i++) {
            if (parsed[i] == ch) return false;   /* duplicate */
        }
        if (count >= OCP_WIFI_REGION_MAX_CHANNELS) return false;
        parsed[count++] = ch;
        if (!comma) break;
        p = comma + 1;
    }

    if (count == 0) return false;
    memcpy(s_active, parsed, count);
    s_active_count = count;
    s_custom = true;
    return true;
}

bool wifi_channels_is_custom(void)
{
    ensure_init();
    return s_custom;
}

const uint8_t *wifi_channels(size_t *count)
{
    ensure_init();
    *count = s_active_count;
    return s_active;
}

void wifi_channels_format_list(char *out, size_t n)
{
    ensure_init();
    size_t len = 0;
    if (!n) return;
    out[0] = '\0';
    for (size_t i = 0; i < s_active_count; i++) {
        int w = snprintf(out + len, n - len, "%s%u", i ? "," : "", (unsigned)s_active[i]);
        if (w < 0 || (size_t)w >= n - len) { out[len] = '\0'; return; }
        len += (size_t)w;
    }
}

void wifi_channels_reset(void)
{
    s_region = NULL;
    s_band = WIFI_BAND_SCOPE_AUTO;
    s_custom = false;
    s_active_count = 0;
    ensure_init();
}
