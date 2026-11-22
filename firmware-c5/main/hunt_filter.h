/*
 * hunt_filter.h — pure-C single-target Signal Hunt helpers (WP-12b).
 *
 * Parses canonical MAC strings, extracts transmitter/BSSID addresses from
 * raw 802.11 frames safely, and applies bounded rate-limiting so pinned-target
 * hunting never floods the OCP UART. Host-tested under ASan/UBSan.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_HUNT_FILTER_H
#define OSCILLA_HUNT_FILTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HUNT_MIN_INTERVAL_MS 100u

/* Parse "aa:bb:cc:dd:ee:ff" (case-insensitive) into 6 bytes. */
bool hunt_parse_mac(const char *s, uint8_t out[6]);

/* True iff `frame` (length `len`, excluding FCS) is a well-formed 802.11
 * management or data frame transmitted by `target_mac` (Addr2 matches). */
bool hunt_wifi_frame_matches(const uint8_t *frame, size_t len, const uint8_t target_mac[6]);

typedef struct {
    uint32_t last_emit_ms;
    bool     has_emitted;
    uint32_t min_interval_ms;
} hunt_rate_limit_t;

void hunt_rate_limit_reset(hunt_rate_limit_t *rl, uint32_t min_interval_ms);
bool hunt_rate_limit_allow(hunt_rate_limit_t *rl, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* OSCILLA_HUNT_FILTER_H */
