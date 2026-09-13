/*
 * wifi_channels.c — see wifi_channels.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_channels.h"

static const uint8_t k_channels[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,        /* 2.4 GHz */
    36, 40, 44, 48, 149, 153, 157, 161, 165,          /* 5 GHz, non-DFS */
};

const uint8_t *wifi_channels(size_t *count)
{
    *count = sizeof k_channels / sizeof k_channels[0];
    return k_channels;
}
