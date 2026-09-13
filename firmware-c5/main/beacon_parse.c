/*
 * beacon_parse.c — see beacon_parse.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "beacon_parse.h"

#include <string.h>

#define FC_TYPE_MASK        0x0C
#define FC_SUBTYPE_MASK     0xF0
#define FC_TYPE_MGMT        0x00
#define FC_SUBTYPE_BEACON   0x80
#define FC_SUBTYPE_PROBE_RSP 0x50

#define IE_SSID             0
#define IE_DS_PARAMS        3
#define IE_RSN              48

#define RSN_CAP_MFPR        (1u << 6)
#define RSN_CAP_MFPC        (1u << 7)

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static uint64_t le64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
    return v;
}

/* RSN fields after the version are each optional from the end (802.11-2020
 * 9.4.2.24), so a short element is valid; a count that overruns it is not. */
static void parse_rsn(const uint8_t *p, size_t len, beacon_info_t *out)
{
    out->has_rsn = true;
    size_t off = 0;

    if (len < 2) { out->rsn_malformed = true; return; }
    off += 2;                                       /* version */

    if (off == len) return;
    if (len - off < 4) { out->rsn_malformed = true; return; }
    off += 4;                                       /* group cipher */

    for (int list = 0; list < 2; list++) {          /* pairwise, then AKM */
        if (off == len) return;
        if (len - off < 2) { out->rsn_malformed = true; return; }
        size_t count = le16(p + off);
        off += 2;
        if (count > (len - off) / 4) { out->rsn_malformed = true; return; }
        off += count * 4;
    }

    if (off == len) return;
    if (len - off < 2) { out->rsn_malformed = true; return; }
    uint16_t caps = le16(p + off);
    out->mfp_capable = (caps & RSN_CAP_MFPC) != 0;
    out->mfp_required = (caps & RSN_CAP_MFPR) != 0;
}

beacon_result_t beacon_parse(const uint8_t *frame, size_t len, beacon_info_t *out)
{
    memset(out, 0, sizeof *out);

    if (len < BEACON_HDR_LEN + BEACON_FIXED_LEN) return BEACON_TOO_SHORT;

    uint8_t fc = frame[0];
    if ((fc & FC_TYPE_MASK) != FC_TYPE_MGMT) return BEACON_NOT_MGMT_BEACON;
    uint8_t sub = fc & FC_SUBTYPE_MASK;
    if (sub != FC_SUBTYPE_BEACON && sub != FC_SUBTYPE_PROBE_RSP) return BEACON_NOT_MGMT_BEACON;

    memcpy(out->bssid, frame + 16, 6);
    const uint8_t *fixed = frame + BEACON_HDR_LEN;
    out->tsf_us = le64(fixed);
    out->interval_tu = le16(fixed + 8);
    out->capability = le16(fixed + 10);

    size_t off = BEACON_HDR_LEN + BEACON_FIXED_LEN;
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
        } else if (id == IE_DS_PARAMS && ie_len >= 1 && out->ds_channel == 0) {
            out->ds_channel = ie[0];
        } else if (id == IE_RSN && !out->has_rsn) {   /* first RSN wins */
            parse_rsn(ie, ie_len, out);
        }
        off += ie_len;
    }
    return BEACON_OK;
}
