/*
 * beacon_parse.c — see beacon_parse.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "beacon_parse.h"

#include <stdio.h>
#include <string.h>

#define FC_TYPE_MASK         0x0C
#define FC_SUBTYPE_MASK      0xF0
#define FC_TYPE_MGMT         0x00
#define FC_SUBTYPE_BEACON    0x80
#define FC_SUBTYPE_PROBE_RSP 0x50

#define IE_SSID              0
#define IE_DS_PARAMS         3
#define IE_COUNTRY           7
#define IE_HT_CAPS           45
#define IE_RSN               48
#define IE_HT_OP             61
#define IE_VHT_CAPS          191
#define IE_VHT_OP            192
#define IE_VENDOR            221
#define IE_EXT               255

#define IE_EXT_HE_CAPS       35
#define IE_EXT_HE_OP         36

#define RSN_CAP_MFPR         (1u << 6)
#define RSN_CAP_MFPC         (1u << 7)

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static uint64_t le64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
    return v;
}

static bool is_ieee_rsn_oui(const uint8_t *p)
{
    return p[0] == 0x00 && p[1] == 0x0F && p[2] == 0xAC;
}

static uint16_t akm_flag_for_suite(const uint8_t *suite)
{
    if (!is_ieee_rsn_oui(suite)) return 0;
    switch (suite[3]) {
        case 1:  return BEACON_AKM_EAP;
        case 2:  return BEACON_AKM_PSK;
        case 3:  return BEACON_AKM_FT_EAP;
        case 4:  return BEACON_AKM_FT_PSK;
        case 5:  return BEACON_AKM_EAP;
        case 6:  return BEACON_AKM_PSK;
        case 8:  return BEACON_AKM_SAE;
        case 9:  return BEACON_AKM_FT_SAE;
        case 18: return BEACON_AKM_OWE;
        default: return 0;
    }
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

    uint16_t akm_mask = 0;
    for (int list = 0; list < 2; list++) {          /* pairwise, then AKM */
        if (off == len) { out->rsn_akm_mask = akm_mask; return; }
        if (len - off < 2) { out->rsn_malformed = true; return; }
        size_t count = le16(p + off);
        off += 2;
        if (count > (len - off) / 4) { out->rsn_malformed = true; return; }
        if (list == 1) {
            for (size_t i = 0; i < count; i++) {
                akm_mask |= akm_flag_for_suite(p + off + i * 4);
            }
        }
        off += count * 4;
    }

    if (off == len) { out->rsn_akm_mask = akm_mask; return; }
    if (len - off < 2) { out->rsn_malformed = true; return; }
    uint16_t caps = le16(p + off);
    out->rsn_akm_mask = akm_mask;
    out->mfp_capable = (caps & RSN_CAP_MFPC) != 0;
    out->mfp_required = (caps & RSN_CAP_MFPR) != 0;
}

static bool is_upper_ascii(uint8_t c) { return c >= 'A' && c <= 'Z'; }

beacon_result_t beacon_parse(const uint8_t *frame, size_t len, beacon_info_t *out)
{
    memset(out, 0, sizeof *out);

    if (len < BEACON_HDR_LEN + BEACON_FIXED_LEN) return BEACON_TOO_SHORT;

    uint8_t fc = frame[0];
    if ((fc & FC_TYPE_MASK) != FC_TYPE_MGMT) return BEACON_NOT_MGMT_BEACON;
    uint8_t sub = fc & FC_SUBTYPE_MASK;
    if (sub != FC_SUBTYPE_BEACON && sub != FC_SUBTYPE_PROBE_RSP) return BEACON_NOT_MGMT_BEACON;

    out->chan_width_mhz = 20;
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
        } else if (id == IE_COUNTRY && ie_len >= 3 && !out->has_country) {
            if (is_upper_ascii(ie[0]) && is_upper_ascii(ie[1])) {
                out->has_country = true;
                out->country[0] = (char)ie[0];
                out->country[1] = (char)ie[1];
                out->country[2] = '\0';
            }
        } else if (id == IE_RSN && !out->has_rsn) {   /* first RSN wins */
            parse_rsn(ie, ie_len, out);
        } else if (id == IE_HT_CAPS && ie_len >= 26) {
            if (out->wifi_gen < 4) out->wifi_gen = 4;
        } else if (id == IE_HT_OP && ie_len >= 2) {
            if (out->wifi_gen < 4) out->wifi_gen = 4;
            if ((ie[1] & 0x03u) != 0 && out->chan_width_mhz < 40) out->chan_width_mhz = 40;
        } else if (id == IE_VHT_CAPS && ie_len >= 12) {
            if (out->wifi_gen < 5) out->wifi_gen = 5;
        } else if (id == IE_VHT_OP && ie_len >= 3) {
            if (out->wifi_gen < 5) out->wifi_gen = 5;
            if (ie[0] == 1 && out->chan_width_mhz < 80) out->chan_width_mhz = 80;
            else if ((ie[0] == 2 || ie[0] == 3) && out->chan_width_mhz < 160) out->chan_width_mhz = 160;
        } else if (id == IE_EXT && ie_len >= 1) {
            if ((ie[0] == IE_EXT_HE_CAPS || ie[0] == IE_EXT_HE_OP) && out->wifi_gen < 6) {
                out->wifi_gen = 6;
            }
        } else if (id == IE_VENDOR && ie_len >= 4) {
            if (ie[0] == 0x00 && ie[1] == 0x50 && ie[2] == 0xF2 && ie[3] == 0x04) {
                out->has_wps = true;
            }
        }
        off += ie_len;
    }
    return BEACON_OK;
}

void beacon_format_akm(uint16_t akm_mask, char *out, size_t n)
{
    static const struct { uint16_t bit; const char *name; } k_akms[] = {
        { BEACON_AKM_PSK,    "PSK"    },
        { BEACON_AKM_SAE,    "SAE"    },
        { BEACON_AKM_EAP,    "EAP"    },
        { BEACON_AKM_FT_PSK, "FT-PSK" },
        { BEACON_AKM_FT_SAE, "FT-SAE" },
        { BEACON_AKM_FT_EAP, "FT-EAP" },
        { BEACON_AKM_OWE,    "OWE"    },
    };
    if (!n) return;
    size_t len = 0;
    out[0] = '\0';
    for (size_t i = 0; i < sizeof k_akms / sizeof k_akms[0]; i++) {
        if ((akm_mask & k_akms[i].bit) == 0) continue;
        int w = snprintf(out + len, n - len, "%s%s", len ? "," : "", k_akms[i].name);
        if (w < 0 || (size_t)w >= n - len) { out[len] = '\0'; return; }
        len += (size_t)w;
    }
}
