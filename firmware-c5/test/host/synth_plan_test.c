/* synth_plan_test.c — host tests for the synthetic-event planner. */
#include "synth_plan.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void run(uint32_t wifi_n, uint32_t ble_n)
{
    synth_plan_t p;
    CHECK(synth_plan_init(&p, wifi_n, ble_n));
    CHECK(synth_plan_total(&p) == wifi_n + ble_n);

    uint8_t *seen_w = calloc(wifi_n + 1, 1), *seen_b = calloc(ble_n + 1, 1);
    uint32_t got_w = 0, got_b = 0, steps = 0, worst_lag = 0;
    synth_lane_t lane;
    uint32_t idx;
    while (synth_plan_next(&p, &lane, &idx)) {
        steps++;
        if (lane == SYNTH_WIFI) { CHECK(idx < wifi_n && !seen_w[idx]); seen_w[idx] = 1; got_w++; }
        else                    { CHECK(idx < ble_n && !seen_b[idx]); seen_b[idx] = 1; got_b++; }
        /* Even spread: each lane's progress stays within one step of its share. */
        if (wifi_n && ble_n) {
            double want_w = (double)steps * wifi_n / (wifi_n + ble_n);
            double lag = want_w > got_w ? want_w - got_w : got_w - want_w;
            if (lag > worst_lag) worst_lag = (uint32_t)(lag + 0.999);
        }
    }
    CHECK(got_w == wifi_n && got_b == ble_n);
    CHECK(steps == wifi_n + ble_n);
    CHECK(worst_lag <= 1);
    CHECK(!synth_plan_next(&p, &lane, &idx));   /* stays finished */
    free(seen_w);
    free(seen_b);
}

int main(void)
{
    run(600, 300);
    run(300, 0);
    run(0, 120);
    run(7, 1000);

    synth_plan_t p;
    CHECK(!synth_plan_init(&p, 0, 0));
    CHECK(!synth_plan_init(&p, SYNTH_MAX_PER_LANE + 1, 1));
    CHECK(synth_plan_init(&p, SYNTH_MAX_PER_LANE, 0));

    /* Addresses: locally administered unicast, lane in octet 2, index below. */
    uint8_t a[6], b[6];
    synth_addr(SYNTH_WIFI, 0x123456, a);
    CHECK(a[0] == 0x02 && a[1] == 0x5e && a[2] == 0 && a[3] == 0x12 && a[4] == 0x34 && a[5] == 0x56);
    CHECK((a[0] & 0x01) == 0 && (a[0] & 0x02) != 0);
    synth_addr(SYNTH_BLE, 0x123456, b);
    CHECK(b[2] == 1 && memcmp(a, b, 6) != 0);
    /* Distinct per index across the whole tested range. */
    for (uint32_t i = 0; i < 1000; i++) {
        uint8_t x[6], y[6];
        synth_addr(SYNTH_WIFI, i, x);
        synth_addr(SYNTH_WIFI, i + 1, y);
        CHECK(memcmp(x, y, 6) != 0);
    }

    if (failures) { printf("synth_plan_test: %d failure(s)\n", failures); return 1; }
    printf("synth_plan_test: ok\n");
    return 0;
}
