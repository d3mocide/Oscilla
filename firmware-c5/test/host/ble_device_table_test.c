/*
 * ble_device_table_test.c — host test for ble_device_table's upsert logic.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ble_device_table.h"

#include <stdio.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, what) do { \
    if (cond) g_pass++; else { g_fail++; printf("  FAIL  %s (line %d)\n", what, __LINE__); } \
} while (0)

static void addr_n(uint8_t out[6], uint8_t n) { memset(out, 0, 6); out[5] = n; }

int main(void)
{
    ble_device_table_t t;
    ble_device_table_reset(&t);
    CHECK(t.count == 0, "reset starts empty");

    {
        uint8_t a[6]; addr_n(a, 1);
        ble_adv_info_t info = {0};
        info.has_name = true;
        info.name_len = 4;
        memcpy(info.name, "Test", 4);
        ble_device_t *row = ble_device_table_upsert(&t, a, &info, -50);
        CHECK(row != NULL, "first sighting inserts a row");
        CHECK(t.count == 1, "count is 1 after one insert");
        CHECK(row->n == 1 && row->rssi == -50, "n and rssi set on insert");
        CHECK(row->has_name && row->name_len == 4 && memcmp(row->name, "Test", 4) == 0, "name captured");
    }
    {
        /* Same address again: updates in place, does not grow the table. */
        uint8_t a[6]; addr_n(a, 1);
        ble_adv_info_t info = {0};
        ble_device_t *row = ble_device_table_upsert(&t, a, &info, -40);
        CHECK(t.count == 1, "a repeat sighting does not grow the table");
        CHECK(row->n == 2, "n increments on a repeat sighting");
        CHECK(row->rssi == -40, "rssi updates to the most recent sighting");
        CHECK(row->has_name, "name from an earlier sighting is sticky when a later one carries none");
    }
    {
        /* is_tracker is sticky once set, even if a later sighting's info doesn't carry it. */
        uint8_t a[6]; addr_n(a, 1);
        ble_adv_info_t info = {0};
        info.is_tracker = true;
        ble_device_table_upsert(&t, a, &info, -60);
        ble_adv_info_t info2 = {0};
        ble_device_t *row = ble_device_table_upsert(&t, a, &info2, -61);
        CHECK(row->is_tracker, "tracker classification stays sticky across later non-tracker sightings");
    }
    {
        /* Fill to capacity, then confirm a new address is dropped, not swapped in. */
        ble_device_table_reset(&t);
        ble_adv_info_t info = {0};
        for (unsigned i = 0; i < BLE_DEVICES_MAX; i++) {
            uint8_t a[6] = {0, 0, 0, 0, (uint8_t)(i >> 8), (uint8_t)i};
            ble_device_table_upsert(&t, a, &info, -50);
        }
        CHECK(t.count == BLE_DEVICES_MAX, "table fills to its cap");

        uint8_t overflow[6] = {1, 2, 3, 4, 5, 6};
        ble_device_t *row = ble_device_table_upsert(&t, overflow, &info, -50);
        CHECK(row == NULL, "an address past capacity is dropped, not inserted");
        CHECK(t.count == BLE_DEVICES_MAX, "count stays at the cap, not incremented on a drop");

        /* An existing address still upserts fine even while the table is full. */
        uint8_t existing[6] = {0, 0, 0, 0, 0, 0};
        row = ble_device_table_upsert(&t, existing, &info, -30);
        CHECK(row != NULL && row->n == 2, "an already-tracked address still upserts once the table is full");
    }

    printf("\n%s: %d passed, %d failed\n", g_fail ? "ble device table test FAILED" : "ble device table test OK",
           g_pass, g_fail);
    return g_fail ? 1 : 0;
}
