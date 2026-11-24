/*
 * ook_pulse.c — see ook_pulse.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ook_pulse.h"

#include <string.h>

#define LEVEL_SILENT (-2)   /* inside a silence that already ended a train */

bool ook_pulse_init(ook_pulse_t *st, uint32_t baud, uint32_t gap_us, uint32_t glitch_us)
{
    if (!st || baud == 0 || baud > 1000000u || gap_us <= glitch_us || gap_us > UINT16_MAX) return false;
    memset(st, 0, sizeof *st);
    st->sample_ns = 1000000000u / baud;
    st->gap_us = gap_us;
    st->glitch_us = glitch_us;
    ook_pulse_reset(st);
    return true;
}

void ook_pulse_reset(ook_pulse_t *st)
{
    if (!st) return;
    st->level = -1;
    st->run_samples = 0;
    st->pend_level = -1;
    st->pend_ns = 0;
    st->in_train = false;
    st->train.n = 0;
    st->train.truncated = false;
}

static uint16_t cap_us(uint64_t ns, uint32_t max_us)
{
    const uint64_t us = (ns + 500) / 1000;
    return (uint16_t)(us > max_us ? max_us : us);
}

static void finish_train(ook_pulse_t *st, uint32_t t_ms, ook_train_cb cb, void *ctx)
{
    if (st->in_train && st->train.n >= OOK_TRAIN_MIN && cb) {
        st->train.end_ms = t_ms;
        cb(&st->train, ctx);
    }
    st->in_train = false;
    st->train.n = 0;
    st->train.truncated = false;
}

/* A filtered run is final: place it in the train. */
static void emit_run(ook_pulse_t *st, int level, uint64_t ns, uint32_t t_ms, ook_train_cb cb, void *ctx)
{
    const uint64_t gap_ns = (uint64_t)st->gap_us * 1000u;
    if (!st->in_train) {
        if (level != 1) return;              /* trains start on carrier */
        st->in_train = true;
        st->train.n = 0;
        st->train.truncated = false;
    }
    if (st->train.n >= OOK_TRAIN_MAX) {
        st->train.truncated = true;
        if (level == 0 && ns >= gap_ns) finish_train(st, t_ms, cb, ctx);
        return;
    }
    st->train.us[st->train.n++] = cap_us(ns, UINT16_MAX);
    if (level == 0 && ns >= gap_ns) {
        st->train.us[st->train.n - 1] = (uint16_t)st->gap_us;
        finish_train(st, t_ms, cb, ctx);
    }
}

/* A raw run ended: glitch-filter it into the pending run. */
static void close_run(ook_pulse_t *st, uint32_t t_ms, ook_train_cb cb, void *ctx)
{
    const uint64_t ns = (uint64_t)st->run_samples * st->sample_ns;
    const int level = st->level;
    if (st->pend_level < 0) {
        if (ns < (uint64_t)st->glitch_us * 1000u) return;   /* a spike out of silence */
        st->pend_level = level;
        st->pend_ns = ns;
    } else if (ns < (uint64_t)st->glitch_us * 1000u || level == st->pend_level) {
        st->pend_ns += ns;                   /* absorb a glitch, or rejoin after one */
    } else {
        emit_run(st, st->pend_level, st->pend_ns, t_ms, cb, ctx);
        st->pend_level = level;
        st->pend_ns = ns;
    }
}

void ook_pulse_feed(ook_pulse_t *st, const uint8_t *data, size_t n, uint32_t t_ms,
                    ook_train_cb cb, void *ctx)
{
    if (!st || !data || st->sample_ns == 0) return;
    const uint64_t gap_ns = (uint64_t)st->gap_us * 1000u;
    for (size_t i = 0; i < n; i++) {
        for (int b = 7; b >= 0; b--) {
            const int bit = (data[i] >> b) & 1;
            if (st->level == LEVEL_SILENT) {
                if (bit == 0) continue;
                st->level = 1;
                st->run_samples = 1;
                continue;
            }
            if (bit == st->level) {
                st->run_samples++;
                if (bit == 1) continue;
                /* A silence that reaches the gap ends the train now, not at the next edge. */
                const uint64_t low_ns = (uint64_t)st->run_samples * st->sample_ns +
                                        (st->pend_level == 0 ? st->pend_ns : 0);
                if (low_ns < gap_ns) continue;
                if (st->pend_level == 1) emit_run(st, 1, st->pend_ns, t_ms, cb, ctx);
                emit_run(st, 0, gap_ns, t_ms, cb, ctx);
                st->pend_level = -1;
                st->pend_ns = 0;
                st->run_samples = 0;
                st->level = LEVEL_SILENT;
                continue;
            }
            if (st->level >= 0) close_run(st, t_ms, cb, ctx);
            st->level = bit;
            st->run_samples = 1;
        }
    }
}
