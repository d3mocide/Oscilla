/* Host tests for hunt_filter.c (WP-12b). */
#include "hunt_filter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    uint8_t mac[6];
    assert(hunt_parse_mac("aa:BB:01:23:45:ff", mac));
    assert(mac[0] == 0xAA && mac[1] == 0xBB && mac[2] == 0x01 &&
           mac[3] == 0x23 && mac[4] == 0x45 && mac[5] == 0xFF);
    assert(!hunt_parse_mac("aa:bb:cc:dd:ee", mac));
    assert(!hunt_parse_mac("aa-bb-cc-dd-ee-ff", mac));
    assert(!hunt_parse_mac("gg:bb:cc:dd:ee:ff", mac));
    assert(!hunt_parse_mac("", mac));

    /* 802.11 management beacon (type=0) matching Addr2 at offset 10..15. */
    uint8_t frame[24];
    memset(frame, 0, sizeof frame);
    frame[0] = 0x80; /* mgmt beacon */
    memcpy(frame + 10, mac, 6);
    assert(hunt_wifi_frame_matches(frame, sizeof frame, mac));
    assert(!hunt_wifi_frame_matches(frame, 23, mac)); /* truncated */

    /* Data frame (type=2) matching Addr2 passes; Control frame (type=1) rejected. */
    frame[0] = 0x08; /* data */
    assert(hunt_wifi_frame_matches(frame, sizeof frame, mac));
    frame[0] = 0xD4; /* control ACK (type=1) */
    assert(!hunt_wifi_frame_matches(frame, sizeof frame, mac));

    /* Mismatched Addr2 rejected. */
    frame[0] = 0x80;
    frame[15] ^= 0x01;
    assert(!hunt_wifi_frame_matches(frame, sizeof frame, mac));

    /* Rate limiter: first emit immediate, suppressed within 100 ms, wraparound safe. */
    hunt_rate_limit_t rl;
    hunt_rate_limit_reset(&rl, 100u);
    assert(hunt_rate_limit_allow(&rl, 1000u));
    assert(!hunt_rate_limit_allow(&rl, 1050u));
    assert(!hunt_rate_limit_allow(&rl, 1099u));
    assert(hunt_rate_limit_allow(&rl, 1100u));
    hunt_rate_limit_reset(&rl, 100u);
    assert(hunt_rate_limit_allow(&rl, 0xFFFFFFC0u));
    assert(!hunt_rate_limit_allow(&rl, 0xFFFFFFF0u));
    assert(hunt_rate_limit_allow(&rl, 0x00000030u));

    printf("hunt filter: MAC parsing, 802.11 Addr2 matching and rate limiter passed\n");
    return 0;
}
