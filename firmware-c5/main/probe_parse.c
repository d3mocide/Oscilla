/*
 * probe_parse.c — see probe_parse.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "probe_parse.h"

#include <string.h>

#define FC_TYPE_MASK          0x0C
#define FC_SUBTYPE_MASK       0xF0
#define FC_TYPE_MGMT          0x00
#define FC_SUBTYPE_PROBE_REQ  0x40

#define IE_SSID               0

probe_result_t probe_req_parse(const uint8_t *frame, size_t len, probe_req_info_t *out)
{
    memset(out, 0, sizeof *out);

    if (len < PROBE_HDR_LEN) return PROBE_TOO_SHORT;

    uint8_t fc = frame[0];
    if ((fc & FC_TYPE_MASK) != FC_TYPE_MGMT) return PROBE_NOT_REQUEST;
    if ((fc & FC_SUBTYPE_MASK) != FC_SUBTYPE_PROBE_REQ) return PROBE_NOT_REQUEST;

    memcpy(out->mac, frame + 10, 6);   /* addr2: transmitter */

    size_t off = PROBE_HDR_LEN;
    while (off < len) {
        if (len - off < 2) { out->ies_truncated = true; break; }
        uint8_t id = frame[off];
        size_t ie_len = frame[off + 1];
        off += 2;
        if (ie_len > len - off) { out->ies_truncated = true; break; }
        const uint8_t *ie = frame + off;

        if (id == IE_SSID && !out->has_ssid) {
            out->has_ssid = true;
            out->ssid_len = (uint8_t)(ie_len > sizeof out->ssid ? sizeof out->ssid : ie_len);
            memcpy(out->ssid, ie, out->ssid_len);
        }
        off += ie_len;
    }
    return PROBE_OK;
}
