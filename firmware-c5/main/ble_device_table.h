/*
 * ble_device_table.h — the BLE device table `scan_bt` replies with
 * (OCP-SPEC §11.2): one row per address seen, deduplicated and capped.
 *
 * Pure C, no IDF dependency: upsert-by-address is the actual logic worth
 * getting wrong, so it's host-testable (firmware-c5/test/host/
 * ble_device_table_test.c) — same split as sniff_track.h for the Wi-Fi
 * sniffer's tables. ble_recon.c is thin glue over this: NimBLE GAP
 * callbacks, the arbiter, and OCP framing.
 *
 * Not thread-safe; the caller (ble_recon.c) owns the lock.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_BLE_DEVICE_TABLE_H
#define OSCILLA_BLE_DEVICE_TABLE_H

#include <stdbool.h>
#include <stdint.h>

#include "ble_adv_parse.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_DEVICES_MAX   96   /* one [BLE] frame, no paging (§11.2) */

typedef struct {
    uint8_t  addr[6];
    bool     has_name;
    uint8_t  name_len;
    uint8_t  name[32];
    bool     has_mfr;
    uint16_t mfr_company_id;
    bool     is_tracker;
    int8_t   rssi;    /* most recent sighting */
    uint32_t n;        /* advertisements seen from this address this scan */
} ble_device_t;

typedef struct {
    ble_device_t devices[BLE_DEVICES_MAX];
    unsigned     count;
} ble_device_table_t;

void ble_device_table_reset(ble_device_table_t *t);

/* Updates the row for `addr` (rssi, most recent name/mfr/tracker info, and
 * `n`) if already present; inserts a new row if not and the table has room.
 * A full table with no existing row for this address is a no-op — lossy by
 * design, same posture as sniff_track's tables. Returns the row (for a
 * caller that wants it), or NULL if the address was dropped. */
ble_device_t *ble_device_table_upsert(ble_device_table_t *t, const uint8_t addr[6],
                                      const ble_adv_info_t *info, int8_t rssi);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_BLE_DEVICE_TABLE_H */
