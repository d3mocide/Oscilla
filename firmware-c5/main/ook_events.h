/*
 * ook_events.h — what an OOK listen session reports (OCP-SPEC §15, D-20).
 *
 * Repeats are merged: a packet equal to one reported within the last
 * window_ms is not reported again, and each repeat restarts that packet's
 * window. A held remote button or a sensor's back-to-back bursts make one
 * event. Undecoded trains are summarised at most once per unknown_ms, with a
 * count of how many arrived since the last summary.
 *
 * Pure C, no allocation, host-tested (firmware-c5/test/host/ook_test.c).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_OOK_EVENTS_H
#define OSCILLA_OOK_EVENTS_H

#include <stdbool.h>
#include <stdint.h>

#include "ook_decode.h"
#include "ook_pulse.h"

#define OOK_EVENTS_RECENT 8
#define OOK_EVENTS_WINDOW_MS 1500u
#define OOK_EVENTS_UNKNOWN_MS 1000u

typedef struct {
    ook_packet_t pk;
    uint32_t t_ms;
    bool used;
} ook_recent_t;

typedef struct {
    ook_recent_t recent[OOK_EVENTS_RECENT];
    uint32_t window_ms;
    uint32_t unknown_ms;
    uint32_t last_unknown_ms;
    bool unknown_sent;
    uint32_t unknown_pending;    /* undecoded trains not yet summarised */
} ook_events_t;

typedef struct {
    uint16_t pulses;             /* entries, excluding the closing gap */
    uint16_t min_us, max_us;
    uint32_t dur_ms;
} ook_train_summary_t;

void ook_events_init(ook_events_t *ev, uint32_t window_ms, uint32_t unknown_ms);

/* True when pk is new and should be reported now. Clock wrap is safe. */
bool ook_events_packet(ook_events_t *ev, const ook_packet_t *pk, uint32_t t_ms);

/* Count one undecoded train. True when a summary is due now; *count then
 * holds the trains it covers (including this one) and the pending count is
 * cleared. */
bool ook_events_unknown(ook_events_t *ev, uint32_t t_ms, uint32_t *count);

void ook_train_summarise(const ook_train_t *t, ook_train_summary_t *out);

#endif /* OSCILLA_OOK_EVENTS_H */
