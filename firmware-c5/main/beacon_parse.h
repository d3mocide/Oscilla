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
    bool     mfp_capable;        /* RSN capabilities bit 7 */
    bool     mfp_required;       /* RSN capabilities bit 6 */

    bool     ies_truncated;      /* an element ran past the frame end */
} beacon_info_t;

typedef enum {
    BEACON_OK = 0,
    BEACON_NOT_MGMT_BEACON = -1, /* not a beacon or probe response */
    BEACON_TOO_SHORT = -2,
} beacon_result_t;

/* `len` excludes the FCS. Never reads outside frame[0, len). */
beacon_result_t beacon_parse(const uint8_t *frame, size_t len, beacon_info_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_BEACON_PARSE_H */
