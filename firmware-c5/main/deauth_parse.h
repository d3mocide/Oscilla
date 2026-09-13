/*
 * deauth_parse.h — bounds-checked parse of an 802.11 deauthentication /
 * disassociation frame (identical body shape: a 2-byte reason code, no
 * information elements).
 *
 * Input is over-the-air bytes any nearby transmitter controls. Pure C, no
 * IDF dependency, fuzzed on the host (test/host/deauth_parse_test.c).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_DEAUTH_PARSE_H
#define OSCILLA_DEAUTH_PARSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEAUTH_HDR_LEN   24   /* 802.11 management header */
#define DEAUTH_BODY_LEN  2    /* reason code only: no IEs in this frame type */

typedef struct {
    uint8_t  dest[6];      /* addr1: the client being dropped (or broadcast) */
    uint8_t  bssid[6];     /* addr3: the network identity claimed */
    uint16_t reason;       /* reason code, as sent (attacker-controlled: not an enum) */
    bool     is_disassoc;  /* false = deauthentication, true = disassociation */
} deauth_info_t;

typedef enum {
    DEAUTH_OK = 0,
    DEAUTH_NOT_DEAUTH = -1,   /* not a deauth/disassoc frame */
    DEAUTH_TOO_SHORT = -2,
} deauth_result_t;

/* `len` excludes the FCS. Never reads outside frame[0, len). */
deauth_result_t deauth_parse(const uint8_t *frame, size_t len, deauth_info_t *out);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_DEAUTH_PARSE_H */
