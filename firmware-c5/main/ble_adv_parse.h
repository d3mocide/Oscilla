/*
 * ble_adv_parse.h — bounds-checked parse of a BLE advertising/scan-response
 * report's AD-structure payload (OCP-SPEC §11).
 *
 * Input is over-the-air bytes any nearby BLE device controls. Pure C, no IDF
 * dependency, so it is host-tested under ASan/UBSan
 * (firmware-c5/test/host/ble_adv_parse_test.c) — same posture as
 * beacon_parse.h for 802.11.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_BLE_ADV_PARSE_H
#define OSCILLA_BLE_ADV_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLE_TRACKER_NONE = 0,
    BLE_TRACKER_AIRTAG,    /* Apple Find My (Mfr 0x004C, type 0x12) */
    BLE_TRACKER_FMDN,      /* Google Find My Device Network (SvcData 0xFEAA, 0x40/0x41) */
    BLE_TRACKER_SMARTTAG,  /* Samsung SmartTag (Svc 0xFD5A) */
    BLE_TRACKER_TILE,      /* Tile (Svc 0xFEED) */
    BLE_TRACKER_CHIPOLO,   /* Chipolo (Svc 0xFE33) */
} ble_tracker_kind_t;

typedef struct {
    bool               has_name;
    uint8_t            name_len;
    uint8_t            name[32];          /* AD type 0x09 (complete) wins over 0x08 (shortened) */

    bool               has_mfr;
    uint16_t           mfr_company_id;    /* AD type 0xFF, first two bytes, little-endian */
    uint8_t            mfr_data_len;      /* bytes after the company ID */
    uint8_t            mfr_data[24];      /* truncated silently past this — classification only needs the front */

    ble_tracker_kind_t tracker;           /* classified tracker family, or BLE_TRACKER_NONE */
    bool               is_tracker;        /* true iff tracker != BLE_TRACKER_NONE */
} ble_adv_info_t;

/* `len` is the AD-structure payload only (event->disc.data in NimBLE terms),
 * not including any GAP header. Never reads outside data[0, len). Always
 * succeeds — a malformed or empty payload just leaves `out` at its zeroed
 * defaults, same "never trust a partial field" posture as gnss_model.cpp. */
void ble_adv_parse(const uint8_t *data, size_t len, ble_adv_info_t *out);

/* Apple Find My network (AirTag and FindMy-compatible accessories):
 * manufacturer company ID 0x004C, payload type byte 0x12 (OCP-SPEC §11.4). */
bool ble_adv_is_airtag(const ble_adv_info_t *info);

/* Wire label (OCP_TRACKER_* in ocp.h), or "" for BLE_TRACKER_NONE. */
const char *ble_tracker_name(ble_tracker_kind_t kind);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_BLE_ADV_PARSE_H */
