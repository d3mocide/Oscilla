/*
 * ook_events.c — see ook_events.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ook_events.h"

#include <string.h>

void ook_events_init(ook_events_t *ev, uint32_t window_ms, uint32_t unknown_ms)
{
    if (!ev) return;
    memset(ev, 0, sizeof *ev);
    ev->window_ms = window_ms;
    ev->unknown_ms = unknown_ms;
}

bool ook_events_packet(ook_events_t *ev, const ook_packet_t *pk, uint32_t t_ms)
{
    if (!ev || !pk) return false;
    ook_recent_t *oldest = &ev->recent[0];
    for (unsigned i = 0; i < OOK_EVENTS_RECENT; i++) {
        ook_recent_t *r = &ev->recent[i];
        if (r->used && ook_packet_same(&r->pk, pk) && (uint32_t)(t_ms - r->t_ms) < ev->window_ms) {
            r->t_ms = t_ms;   /* a repeat keeps the window open */
            return false;
        }
        if (!r->used) oldest = r;
        else if (oldest->used && (uint32_t)(t_ms - r->t_ms) > (uint32_t)(t_ms - oldest->t_ms)) oldest = r;
    }
    oldest->pk = *pk;
    oldest->t_ms = t_ms;
    oldest->used = true;
    return true;
}

bool ook_events_unknown(ook_events_t *ev, uint32_t t_ms, uint32_t *count)
{
    if (count) *count = 0;
    if (!ev) return false;
    if (ev->unknown_pending < UINT32_MAX) ev->unknown_pending++;
    if (ev->unknown_sent && (uint32_t)(t_ms - ev->last_unknown_ms) < ev->unknown_ms) return false;
    ev->unknown_sent = true;
    ev->last_unknown_ms = t_ms;
    if (count) *count = ev->unknown_pending;
    ev->unknown_pending = 0;
    return true;
}

void ook_train_summarise(const ook_train_t *t, ook_train_summary_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof *out);
    if (!t || t->n == 0 || t->n > OOK_TRAIN_MAX) return;
    /* A train closed by a gap ends on that (capped) low; it is not a pulse. */
    const uint16_t n = (t->n % 2 == 0 && !t->truncated) ? (uint16_t)(t->n - 1) : t->n;
    uint32_t sum_us = 0;
    out->min_us = UINT16_MAX;
    for (uint16_t i = 0; i < n; i++) {
        if (t->us[i] < out->min_us) out->min_us = t->us[i];
        if (t->us[i] > out->max_us) out->max_us = t->us[i];
        sum_us += t->us[i];
    }
    if (n == 0) out->min_us = 0;
    out->pulses = n;
    out->dur_ms = (sum_us + 500u) / 1000u;
}
