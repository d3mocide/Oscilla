/*
 * hunt_filter.c — see hunt_filter.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "hunt_filter.h"

#include <string.h>

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool hunt_parse_mac(const char *s, uint8_t out[6])
{
    if (!s || !out) return false;
    if (strlen(s) != 17) return false;
    for (size_t i = 0; i < 6; i++) {
        int hi = hex_val(s[i * 3]);
        int lo = hex_val(s[i * 3 + 1]);
        if (hi < 0 || lo < 0) return false;
        if (i < 5 && s[i * 3 + 2] != ':') return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

bool hunt_wifi_frame_matches(const uint8_t *frame, size_t len, const uint8_t target_mac[6])
{
    /* Minimum 802.11 mgmt/data header with Addr1+Addr2+Addr3 is 24 bytes.
     * Frame Control bits 2..3: type 0 = management, type 2 = data. */
    if (!frame || !target_mac || len < 24) return false;
    uint8_t ftype = (uint8_t)((frame[0] >> 2) & 0x03u);
    if (ftype != 0u && ftype != 2u) return false;
    /* Addr2 (Transmitter Address) sits at bytes 10..15 in both mgmt and data frames. */
    return memcmp(frame + 10, target_mac, 6) == 0;
}

void hunt_rate_limit_reset(hunt_rate_limit_t *rl, uint32_t min_interval_ms)
{
    if (!rl) return;
    rl->last_emit_ms = 0;
    rl->has_emitted = false;
    rl->min_interval_ms = min_interval_ms;
}

bool hunt_rate_limit_allow(hunt_rate_limit_t *rl, uint32_t now_ms)
{
    if (!rl) return false;
    if (!rl->has_emitted || (uint32_t)(now_ms - rl->last_emit_ms) >= rl->min_interval_ms) {
        rl->last_emit_ms = now_ms;
        rl->has_emitted = true;
        return true;
    }
    return false;
}
