/*
 * wifi_channels.c — see wifi_channels.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_channels.h"

#include <stddef.h>

/* Written only by set_region while no PHY tool runs (ocp_server.c), read
 * by an engine's start; an aligned pointer store, so no lock. */
static const ocp_wifi_region_t *s_region;

const ocp_wifi_region_t *wifi_region(void)
{
    if (!s_region) s_region = ocp_wifi_region_find(OCP_WIFI_REGION_DEFAULT);
    return s_region;
}

void wifi_region_select(const ocp_wifi_region_t *region)
{
    if (region) s_region = region;
}

const uint8_t *wifi_channels(size_t *count)
{
    const ocp_wifi_region_t *r = wifi_region();
    *count = r->count;
    return r->channels;
}
