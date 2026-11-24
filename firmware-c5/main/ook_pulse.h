/*
 * ook_pulse.h — demodulated OOK bitstream to pulse trains (DESIGN §6.1).
 *
 * Input is the CC1101 FIFO sampling stream (cc1101_capture): one bit per
 * sample at a fixed baud, MSB first, 1 = carrier. Output is a train of
 * alternating durations in microseconds, high first:
 *     us[0] high, us[1] low, us[2] high, ...
 * A train ends when a low run reaches gap_us. That low is stored as the
 * last entry, capped at gap_us. Runs shorter than glitch_us are absorbed
 * into the surrounding level, so one-sample noise does not split a pulse.
 *
 * Pure C, no allocation, host-tested (firmware-c5/test/host/ook_test.c).
 * Decoded bytes stay hostile: a train is untrusted radio input.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_OOK_PULSE_H
#define OSCILLA_OOK_PULSE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OOK_TRAIN_MAX 512   /* durations; a longer train is cut and flagged */
#define OOK_TRAIN_MIN 8     /* shorter trains are noise and are not emitted */

typedef struct {
    uint16_t n;                     /* entries in us[] (even when it ends on a gap) */
    bool truncated;                 /* hit OOK_TRAIN_MAX before a gap */
    uint32_t end_ms;                /* caller's clock for the chunk that ended it */
    uint16_t us[OOK_TRAIN_MAX];
} ook_train_t;

typedef void (*ook_train_cb)(const ook_train_t *train, void *ctx);

typedef struct {
    /* configuration */
    uint32_t sample_ns;             /* 1e9 / baud */
    uint32_t gap_us;
    uint32_t glitch_us;
    /* raw run in progress */
    int level;                      /* -1 before the first sample */
    uint32_t run_samples;
    /* filtered run waiting for its successor */
    int pend_level;                 /* -1 when empty */
    uint64_t pend_ns;
    /* train being built */
    ook_train_t train;
    bool in_train;
} ook_pulse_t;

/* false on nonsense configuration (baud 0, gap <= glitch, gap > 65535 us). */
bool ook_pulse_init(ook_pulse_t *st, uint32_t baud, uint32_t gap_us, uint32_t glitch_us);

/* Drop any partial train (after a FIFO overflow or a capture restart). */
void ook_pulse_reset(ook_pulse_t *st);

/* Feed n bytes sampled at the configured baud; cb runs for each finished train. */
void ook_pulse_feed(ook_pulse_t *st, const uint8_t *data, size_t n, uint32_t t_ms,
                    ook_train_cb cb, void *ctx);

#endif /* OSCILLA_OOK_PULSE_H */
