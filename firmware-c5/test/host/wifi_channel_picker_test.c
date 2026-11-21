/*
 * wifi_channel_picker_test.c — host tests for D-UCB adaptive channel picker
 * (firmware-c5/main/wifi_channel_picker.c, D-6).
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_channel_picker.h"

#include <stdio.h>

static int failures;
#define CHECK(cond, what) do { if (!(cond)) { printf("FAIL: %s\n", what); failures++; } } while (0)

int main(void)
{
    wifi_channel_picker_t p;

    /* 1. Single channel never goes out of bounds. */
    wifi_channel_picker_reset(&p, 1);
    for (int t = 0; t < 20; t++) {
        size_t next = wifi_channel_picker_step(&p, 0, (uint32_t)t);
        CHECK(next == 0, "single channel stays at 0");
    }

    /* 2. Initial sweep visits every channel 0..N-1 in order. */
    const size_t N = 11;
    wifi_channel_picker_reset(&p, N);
    size_t idx = 0;
    for (size_t step = 0; step + 1 < N; step++) {
        idx = wifi_channel_picker_step(&p, idx, 0);
        CHECK(idx == step + 1, "initial sweep covers every channel");
    }

    /* 3. Quiet environment (all zeros) stays balanced — no starvation. */
    wifi_channel_picker_reset(&p, N);
    idx = 0;
    for (int t = 0; t < 220; t++) {
        idx = wifi_channel_picker_step(&p, idx, 0);
    }
    uint32_t min_v = p.visits[0], max_v = p.visits[0];
    for (size_t i = 1; i < N; i++) {
        if (p.visits[i] < min_v) min_v = p.visits[i];
        if (p.visits[i] > max_v) max_v = p.visits[i];
    }
    CHECK(min_v >= 15 && max_v <= 25, "quiet environment stays balanced across all arms");

    /* 4. Busy channel earns significantly more dwell while quiet arms stay alive. */
    wifi_channel_picker_reset(&p, N);
    idx = 0;
    for (int t = 0; t < 300; t++) {
        uint32_t hits = (idx == 5) ? 10 : 0;   /* ch index 5 is busy */
        idx = wifi_channel_picker_step(&p, idx, hits);
    }
    uint32_t busy_v = p.visits[5];
    uint32_t min_quiet = 999999;
    for (size_t i = 0; i < N; i++) {
        if (i != 5 && p.visits[i] < min_quiet) min_quiet = p.visits[i];
    }
    CHECK(busy_v >= min_quiet * 3, "busy channel gets >= 3x quiet channel visits");
    CHECK(min_quiet >= 5, "quiet channels are still periodically revisited (starvation-free)");

    /* 5. Non-stationary shift: deep busy history on arm 0, then arm 0 goes dead
     * and moderate traffic appears on arm 10. Discounting must let arm 10 beat
     * the stale dead arm; undiscounted UCB1 stays trapped on arm 0. */
    wifi_channel_picker_reset(&p, N);
    idx = 0;
    for (int t = 0; t < 400; t++) {
        idx = wifi_channel_picker_step(&p, idx, idx == 0 ? 12 : 0);
    }
    uint32_t arm0_before = p.visits[0];
    uint32_t arm10_before = p.visits[10];
    for (int t = 0; t < 120; t++) {
        idx = wifi_channel_picker_step(&p, idx, idx == 10 ? 2 : 0);
    }
    uint32_t arm0_after = p.visits[0] - arm0_before;
    uint32_t arm10_after = p.visits[10] - arm10_before;
    CHECK(arm10_after > arm0_after && arm0_after < 25,
          "discounted UCB abandons dead channel for moderate new traffic");

    if (failures) { printf("wifi channel picker: %d failure(s)\n", failures); return 1; }
    printf("wifi channel picker: D-UCB sweep, busy focus, starvation-freedom and shift adaptation passed\n");
    return 0;
}
