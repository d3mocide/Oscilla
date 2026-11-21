/*
 * beacon_parse.h — bounds-checked parse of an 802.11 beacon / probe response.
 *
 * Input is over-the-air bytes any nearby transmitter controls. Pure C, no IDF
 * dependency, so it is fuzzed on the host under ASan/UBSan
 * (firmware-c5/test/host/beacon_test.c).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_BEACON_PARSE_H
#define OSCILLA_BEACON_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BEACON_HDR_LEN      24   /* 802.11 management header */
#define BEACON_FIXED_LEN    12   /* timestamp, interval, capability */

/* Recognized IEEE 802.11 (OUI 00:0F:AC) RSN AKM suite bitmask flags. */
#define BEACON_AKM_EAP      (1u << 0)  /* 802.1X / 802.1X-SHA256 (types 1, 5) */
#define BEACON_AKM_PSK      (1u << 1)  /* PSK / PSK-SHA256 (types 2, 6) */
#define BEACON_AKM_FT_EAP   (1u << 2)  /* FT-802.1X (type 3) */
#define BEACON_AKM_FT_PSK   (1u << 3)  /* FT-PSK (type 4) */
#define BEACON_AKM_SAE      (1u << 4)  /* WPA3-SAE (type 8) */
#define BEACON_AKM_FT_SAE   (1u << 5)  /* FT-SAE (type 9) */
#define BEACON_AKM_OWE      (1u << 6)  /* Enhanced Open (type 18) */

typedef struct {
    uint8_t  bssid[6];
    uint64_t tsf_us;             /* beacon timestamp */
    uint16_t interval_tu;        /* 1 TU = 1024 us */
    uint16_t capability;

    bool     has_ssid;
    uint8_t  ssid_len;
    uint8_t  ssid[32];

    uint8_t  ds_channel;         /* DS Parameter Set; 0 if absent */

    bool     has_rsn;            /* an RSN element was present */
    bool     rsn_malformed;      /* ...but its lengths didn't add up */
    uint16_t rsn_akm_mask;       /* BEACON_AKM_* bitmask (valid when !rsn_malformed) */
    bool     mfp_capable;        /* RSN capabilities bit 7 */
    bool     mfp_required;       /* RSN capabilities bit 6 */

    uint8_t  wifi_gen;           /* 0=legacy, 4=HT, 5=VHT, 6=HE */
    uint8_t  chan_width_mhz;     /* 20, 40, 80, or 160 */
    bool     has_country;
    char     country[3];         /* 2 uppercase ASCII letters + NUL */
    bool     has_wps;            /* Wi-Fi Alliance WPS vendor IE (00:50:F2:04) */

    bool     ies_truncated;      /* an element ran past the frame end */
} beacon_info_t;

typedef enum {
    BEACON_OK = 0,
    BEACON_NOT_MGMT_BEACON = -1, /* not a beacon or probe response */
    BEACON_TOO_SHORT = -2,
} beacon_result_t;

/* `len` excludes the FCS. Never reads outside frame[0, len). */
beacon_result_t beacon_parse(const uint8_t *frame, size_t len, beacon_info_t *out);

/* Format `rsn_akm_mask` as a comma-separated bare OCP value (e.g. "PSK,SAE"). */
void beacon_format_akm(uint16_t akm_mask, char *out, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_BEACON_PARSE_H */
