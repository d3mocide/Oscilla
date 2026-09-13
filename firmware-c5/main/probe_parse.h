/*
 * probe_parse.h — bounds-checked parse of an 802.11 probe request's
 * transmitter MAC and (optional) SSID IE.
 *
 * A companion to beacon_parse.h, kept separate: a probe request has no
 * 12-byte fixed field before its IEs (beacons/probe responses do), so the IE
 * offset differs and reusing beacon_parse's beacon_info_t would carry fields
 * that don't apply.
 *
 * Input is over-the-air bytes any nearby transmitter controls. Pure C, no IDF
 * dependency, so it is fuzzed on the host under ASan/UBSan
 * (firmware-c5/test/host/probe_parse_test.c).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_PROBE_PARSE_H
#define OSCILLA_PROBE_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROBE_HDR_LEN        24   /* 802.11 management header */

typedef struct {
    uint8_t  mac[6];             /* addr2: the probing device (transmitter) */

    bool     has_ssid;           /* an SSID element was present */
    uint8_t  ssid_len;           /* 0 for a wildcard/broadcast probe */
    uint8_t  ssid[32];

    bool     ies_truncated;      /* an element ran past the frame end */
} probe_req_info_t;

typedef enum {
    PROBE_OK = 0,
    PROBE_NOT_REQUEST = -1,      /* not a probe request */
    PROBE_TOO_SHORT = -2,
} probe_result_t;

/* `len` excludes the FCS. Never reads outside frame[0, len). */
probe_result_t probe_req_parse(const uint8_t *frame, size_t len, probe_req_info_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_PROBE_PARSE_H */
