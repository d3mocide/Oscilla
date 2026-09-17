/* zig_frame_test.c — hostile MAC header fixtures for D-17. */
#include <stdio.h>
#include <string.h>
#include "zig_frame.h"
#include "zig_table.h"

static int fail;
static void check(int ok, const char *s) { printf("  %s  %s\n", ok ? "PASS" : "FAIL", s); if (!ok) fail = 1; }
int main(void)
{
    /* length=11; data frame, PAN compression, short source 0x1234, PAN 0x1a2b */
    uint8_t good[] = {11, 0x41, 0x88, 7, 0x2b, 0x1a, 0xff, 0xff, 0x34, 0x12, 0, 0};
    zig_frame_t f; check(zig_frame_parse(good, sizeof good, &f), "short-address MAC header parses");
    check(f.pan_id == 0x1a2b && f.short_addr == 0x1234 && f.has_short, "PAN and short source retained");
    good[0] = 127; check(!zig_frame_parse(good, sizeof good, &f), "claimed overlong PHY frame rejected");
    zig_table_t t; zig_table_clear(&t);
    check(zig_table_upsert(&t, 0x1a2b, "802154", 11, 0x1234, true, NULL, false, -60, 91, 10), "first node inserts");
    check(!zig_table_upsert(&t, 0x1a2b, "802154", 11, 0x1234, true, NULL, false, -50, 99, 20), "repeat updates without a first-sighting event");
    check(t.pan_count == 1 && t.node_count == 1 && t.nodes[0].rssi == -50, "table stays deduplicated and updates signal");
    printf("\nzig frame test %s\n", fail ? "FAILED" : "OK"); return fail;
}
