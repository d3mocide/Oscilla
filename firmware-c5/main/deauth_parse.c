/*
 * deauth_parse.c — see deauth_parse.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "deauth_parse.h"

#include <string.h>

#define FC_TYPE_MASK         0x0C
#define FC_SUBTYPE_MASK      0xF0
#define FC_TYPE_MGMT         0x00
#define FC_SUBTYPE_DEAUTH    0xC0
#define FC_SUBTYPE_DISASSOC  0xA0

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

deauth_result_t deauth_parse(const uint8_t *frame, size_t len, deauth_info_t *out)
{
    memset(out, 0, sizeof *out);

    if (len < DEAUTH_HDR_LEN + DEAUTH_BODY_LEN) return DEAUTH_TOO_SHORT;

    uint8_t fc = frame[0];
    if ((fc & FC_TYPE_MASK) != FC_TYPE_MGMT) return DEAUTH_NOT_DEAUTH;
    uint8_t sub = fc & FC_SUBTYPE_MASK;
    if (sub != FC_SUBTYPE_DEAUTH && sub != FC_SUBTYPE_DISASSOC) return DEAUTH_NOT_DEAUTH;

    memcpy(out->dest, frame + 4, 6);    /* addr1 */
    memcpy(out->bssid, frame + 16, 6);  /* addr3 */
    out->reason = le16(frame + DEAUTH_HDR_LEN);
    out->is_disassoc = (sub == FC_SUBTYPE_DISASSOC);
    return DEAUTH_OK;
}
