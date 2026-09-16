/*
 * ble_device_table.c — see ble_device_table.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ble_device_table.h"

#include <string.h>

void ble_device_table_reset(ble_device_table_t *t)
{
    memset(t, 0, sizeof *t);
}

static ble_device_t *find(ble_device_table_t *t, const uint8_t addr[6])
{
    for (unsigned i = 0; i < t->count; i++) {
        if (memcmp(t->devices[i].addr, addr, 6) == 0) return &t->devices[i];
    }
    return NULL;
}

ble_device_t *ble_device_table_upsert(ble_device_table_t *t, const uint8_t addr[6],
                                      const ble_adv_info_t *info, int8_t rssi)
{
    ble_device_t *row = find(t, addr);
    if (!row) {
        if (t->count >= BLE_DEVICES_MAX) return NULL;   /* full: drop, don't replace (lossy by design) */
        row = &t->devices[t->count++];
        memcpy(row->addr, addr, 6);
        row->n = 0;
    }

    row->rssi = rssi;
    row->n++;
    if (info->has_name) {
        row->has_name = true;
        row->name_len = info->name_len;
        memcpy(row->name, info->name, info->name_len);
    }
    if (info->has_mfr) {
        row->has_mfr = true;
        row->mfr_company_id = info->mfr_company_id;
    }
    if (info->is_tracker) row->is_tracker = true;   /* sticky: one tracker sighting marks the row */

    return row;
}
