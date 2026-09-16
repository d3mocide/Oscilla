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

#define AD_TYPE_SHORT_NAME     0x08
#define AD_TYPE_COMPLETE_NAME  0x09
#define AD_TYPE_MFR_DATA       0xFF

#define APPLE_COMPANY_ID       0x004C
#define APPLE_FINDMY_TYPE      0x12

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
            out->mfr_company_id = (uint16_t)(ad_data[0] | (ad_data[1] << 8));
            size_t payload_len = ad_data_len - 2;
            out->mfr_data_len = (uint8_t)(payload_len > sizeof out->mfr_data ? sizeof out->mfr_data : payload_len);
            memcpy(out->mfr_data, ad_data + 2, out->mfr_data_len);
        }

        off += (size_t)ad_len + 1;
    }

    out->is_tracker = ble_adv_is_airtag(out);
}

bool ble_adv_is_airtag(const ble_adv_info_t *info)
{
    return info->has_mfr && info->mfr_company_id == APPLE_COMPANY_ID &&
           info->mfr_data_len >= 1 && info->mfr_data[0] == APPLE_FINDMY_TYPE;
}
