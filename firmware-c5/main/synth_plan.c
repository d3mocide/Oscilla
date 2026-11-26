/*
 * synth_plan.c — see synth_plan.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "synth_plan.h"

#include <stddef.h>

bool synth_plan_init(synth_plan_t *p, uint32_t wifi_n, uint32_t ble_n)
{
    if (!p || wifi_n > SYNTH_MAX_PER_LANE || ble_n > SYNTH_MAX_PER_LANE) return false;
    if (wifi_n == 0 && ble_n == 0) return false;
    *p = (synth_plan_t){ .wifi_n = wifi_n, .ble_n = ble_n };
    return true;
}

uint32_t synth_plan_total(const synth_plan_t *p)
{
    return p->wifi_n + p->ble_n;
}

bool synth_plan_next(synth_plan_t *p, synth_lane_t *lane, uint32_t *index)
{
    const bool wifi_left = p->wifi_done < p->wifi_n;
    const bool ble_left = p->ble_done < p->ble_n;
    if (!wifi_left && !ble_left) return false;

    /* Take Wi-Fi while its completed fraction trails BLE's (cross-multiplied,
     * 64-bit: counts are up to 24 bits each). */
    bool wifi = wifi_left;
    if (wifi_left && ble_left) {
        wifi = (uint64_t)p->wifi_done * p->ble_n <= (uint64_t)p->ble_done * p->wifi_n;
    }
    if (wifi) {
        *lane = SYNTH_WIFI;
        *index = p->wifi_done++;
    } else {
        *lane = SYNTH_BLE;
        *index = p->ble_done++;
    }
    return true;
}

void synth_addr(synth_lane_t lane, uint32_t index, uint8_t out[6])
{
    out[0] = 0x02;   /* locally administered, unicast */
    out[1] = 0x5e;
    out[2] = (uint8_t)lane;
    out[3] = (uint8_t)(index >> 16);
    out[4] = (uint8_t)(index >> 8);
    out[5] = (uint8_t)index;
}
