/*
 * synth_plan.h — pure planning for the synthetic-event bench build
 * (CONFIG_OSCILLA_SYNTH_BENCH). Decides which made-up network or BLE device
 * comes next and what its address is. No radio, no I/O: host-tested.
 *
 * Addresses are locally administered unicast (first octet 0x02) under a fixed
 * 02:5e:<lane> prefix, so a synthetic row can never be mistaken for a real one
 * in a log: 02:5e:00:xx:xx:xx for Wi-Fi BSSIDs, 02:5e:01:xx:xx:xx for BLE.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_SYNTH_PLAN_H
#define OSCILLA_SYNTH_PLAN_H

#include <stdbool.h>
#include <stdint.h>

#define SYNTH_MAX_PER_LANE 0xFFFFFFu   /* three address octets */

typedef enum { SYNTH_WIFI = 0, SYNTH_BLE = 1 } synth_lane_t;

typedef struct {
    uint32_t wifi_n, ble_n;    /* distinct newcomers to introduce per lane */
    uint32_t wifi_done, ble_done;
} synth_plan_t;

/* False when either count exceeds SYNTH_MAX_PER_LANE or both are zero. */
bool synth_plan_init(synth_plan_t *p, uint32_t wifi_n, uint32_t ble_n);

/* Next newcomer, spread so each lane finishes at the same time. False once
 * every newcomer has been handed out. */
bool synth_plan_next(synth_plan_t *p, synth_lane_t *lane, uint32_t *index);

uint32_t synth_plan_total(const synth_plan_t *p);

void synth_addr(synth_lane_t lane, uint32_t index, uint8_t out[6]);

#endif /* OSCILLA_SYNTH_PLAN_H */
