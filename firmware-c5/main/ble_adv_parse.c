/*
 * ble_adv_parse.c — see ble_adv_parse.h.
 *
 * AD structure: `[len][type][data[len-1]]` repeated until the buffer ends; a
 * `len` of 0 is padding and ends the walk (BLE Core Spec, Supplement Part A
 * §1). Unlike 802.11 IEs (beacon_parse.c), `len` covers the type byte too,
 * not just the data that follows it.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ble_adv_parse.h"

#include <string.h>

#include "ocp.h"

#define AD_TYPE_UUID16_INCOMP  0x02
#define AD_TYPE_UUID16_COMP    0x03
#define AD_TYPE_SHORT_NAME     0x08
#define AD_TYPE_COMPLETE_NAME  0x09
#define AD_TYPE_SERVICE_DATA16 0x16
#define AD_TYPE_MFR_DATA       0xFF

#define APPLE_COMPANY_ID       0x004C
#define APPLE_FINDMY_TYPE      0x12

#define UUID16_FMDN_EDDYSTONE  0xFEAA
#define FMDN_FRAME_UNWANTED    0x40
#define FMDN_FRAME_NORMAL      0x41

#define UUID16_SMARTTAG        0xFD5A
#define UUID16_TILE            0xFEED
#define UUID16_CHIPOLO         0xFE33

static uint16_t le16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static ble_tracker_kind_t classify_uuid16(uint16_t uuid)
{
    switch (uuid) {
        case UUID16_SMARTTAG: return BLE_TRACKER_SMARTTAG;
        case UUID16_TILE:     return BLE_TRACKER_TILE;
        case UUID16_CHIPOLO:  return BLE_TRACKER_CHIPOLO;
        default:              return BLE_TRACKER_NONE;
    }
}

void ble_adv_parse(const uint8_t *data, size_t len, ble_adv_info_t *out)
{
    memset(out, 0, sizeof *out);

    size_t off = 0;
    while (off < len) {
        uint8_t ad_len = data[off];
        if (ad_len == 0) break;                       /* padding: nothing valid follows */
        size_t remaining = len - off - 1;              /* bytes after the length byte itself */
        if (ad_len > remaining) break;                  /* would run past the buffer */

        uint8_t ad_type = data[off + 1];
        const uint8_t *ad_data = data + off + 2;
        size_t ad_data_len = ad_len - 1;         /* len includes the type byte */

        if ((ad_type == AD_TYPE_COMPLETE_NAME || ad_type == AD_TYPE_SHORT_NAME) &&
            (!out->has_name || ad_type == AD_TYPE_COMPLETE_NAME)) {
            out->has_name = true;
            out->name_len = (uint8_t)(ad_data_len > sizeof out->name ? sizeof out->name : ad_data_len);
            memcpy(out->name, ad_data, out->name_len);
        } else if (ad_type == AD_TYPE_MFR_DATA && ad_data_len >= 2 && !out->has_mfr) {
            out->has_mfr = true;
            out->mfr_company_id = le16(ad_data);
            size_t payload_len = ad_data_len - 2;
            out->mfr_data_len = (uint8_t)(payload_len > sizeof out->mfr_data ? sizeof out->mfr_data : payload_len);
            memcpy(out->mfr_data, ad_data + 2, out->mfr_data_len);
            if (out->tracker == BLE_TRACKER_NONE && ble_adv_is_airtag(out)) {
                out->tracker = BLE_TRACKER_AIRTAG;
            }
        } else if (ad_type == AD_TYPE_SERVICE_DATA16 && ad_data_len >= 2 &&
                   out->tracker == BLE_TRACKER_NONE) {
            uint16_t uuid = le16(ad_data);
            if (uuid == UUID16_FMDN_EDDYSTONE && ad_data_len >= 3 &&
                (ad_data[2] == FMDN_FRAME_UNWANTED || ad_data[2] == FMDN_FRAME_NORMAL)) {
                out->tracker = BLE_TRACKER_FMDN;
            } else {
                out->tracker = classify_uuid16(uuid);
            }
        } else if ((ad_type == AD_TYPE_UUID16_INCOMP || ad_type == AD_TYPE_UUID16_COMP) &&
                   out->tracker == BLE_TRACKER_NONE) {
            for (size_t i = 0; i + 2 <= ad_data_len; i += 2) {
                ble_tracker_kind_t k = classify_uuid16(le16(ad_data + i));
                if (k != BLE_TRACKER_NONE) {
                    out->tracker = k;
                    break;
                }
            }
        }

        off += (size_t)ad_len + 1;
    }

    out->is_tracker = (out->tracker != BLE_TRACKER_NONE);
}

bool ble_adv_is_airtag(const ble_adv_info_t *info)
{
    return info->has_mfr && info->mfr_company_id == APPLE_COMPANY_ID &&
           info->mfr_data_len >= 1 && info->mfr_data[0] == APPLE_FINDMY_TYPE;
}

const char *ble_tracker_name(ble_tracker_kind_t kind)
{
    switch (kind) {
        case BLE_TRACKER_AIRTAG:   return OCP_TRACKER_AIRTAG;
        case BLE_TRACKER_FMDN:     return OCP_TRACKER_FMDN;
        case BLE_TRACKER_SMARTTAG: return OCP_TRACKER_SMARTTAG;
        case BLE_TRACKER_TILE:     return OCP_TRACKER_TILE;
        case BLE_TRACKER_CHIPOLO:  return OCP_TRACKER_CHIPOLO;
        case BLE_TRACKER_NONE:     break;
    }
    return "";
}
