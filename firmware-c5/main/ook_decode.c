/*
 * ook_decode.c — see ook_decode.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ook_decode.h"

#include <string.h>

#define PRINCETON_BITS 24
#define PRINCETON_SPAN (2 * PRINCETON_BITS + 2)   /* pairs + sync high + separator */
#define PRINCETON_TE_MIN 80u
#define PRINCETON_TE_MAX 700u

#define LACROSSE_BITS 40
#define LACROSSE_SYNC_MIN_US 600u
#define LACROSSE_SYNC_MAX_US 1000u
#define LACROSSE_SYNC_PAIRS 3
#define LACROSSE_PERIOD_MIN_US 500u
#define LACROSSE_PERIOD_MAX_US 950u
#define LACROSSE_PULSE_MIN_US 120u

const char *ook_proto_name(ook_proto_t proto)
{
    switch (proto) {
    case OOK_PROTO_PRINCETON: return "princeton";
    case OOK_PROTO_LACROSSE_TX141THBV2: return "lacrosse_tx141thbv2";
    default: return "none";
    }
}

bool ook_packet_same(const ook_packet_t *a, const ook_packet_t *b)
{
    return a && b && a->proto == b->proto && a->nbits == b->nbits &&
           memcmp(a->data, b->data, (a->nbits + 7u) / 8u) == 0;
}

/* Merge into out[] (same frame -> frames++), else append. */
static size_t add_packet(ook_packet_t *out, size_t cap, size_t n, const ook_packet_t *p)
{
    for (size_t i = 0; i < n; i++) {
        if (ook_packet_same(&out[i], p)) {
            if (out[i].frames < UINT8_MAX) out[i].frames++;
            return n;
        }
    }
    if (n >= cap) return n;
    out[n] = *p;
    out[n].frames = 1;
    return n + 1;
}

static void set_bit(uint8_t *data, unsigned i, bool v)
{
    if (v) data[i / 8] |= (uint8_t)(0x80u >> (i % 8));
}

/* ---- Princeton / EV1527 ------------------------------------------------ */

/* Parse 24 pairs at us[0..47], sync high us[48], separator us[49]. */
static bool princeton_frame(const uint16_t *us, ook_packet_t *p)
{
    uint32_t lo = UINT32_MAX, hi = 0;
    for (unsigned i = 0; i < 2 * PRINCETON_BITS; i++) {
        if (us[i] < lo) lo = us[i];
        if (us[i] > hi) hi = us[i];
    }
    if (lo == 0) return false;
    const uint32_t mid = (lo + hi) / 2;
    uint32_t s_sum = 0, s_n = 0, l_sum = 0, l_n = 0;
    for (unsigned i = 0; i < 2 * PRINCETON_BITS; i++) {
        if (us[i] < mid) { s_sum += us[i]; s_n++; }
        else { l_sum += us[i]; l_n++; }
    }
    if (s_n != PRINCETON_BITS || l_n != PRINCETON_BITS) return false;   /* one short, one long per bit */
    const uint32_t s = s_sum / s_n, l = l_sum / l_n;
    /* long/short in [2.4, 4.5]: Princeton is nominally 1:3; LaCrosse PWM (~1:2) must not pass. */
    if (l * 10 < s * 24 || l * 10 > s * 45) return false;
    for (unsigned i = 0; i < 2 * PRINCETON_BITS; i++) {                   /* each within +-40% of its class */
        const uint32_t ref = us[i] < mid ? s : l;
        if (us[i] * 10u < ref * 6u || us[i] * 10u > ref * 14u) return false;
    }
    const uint32_t te = (s + l) / 4;
    if (te < PRINCETON_TE_MIN || te > PRINCETON_TE_MAX) return false;
    const uint32_t sync_hi = us[2 * PRINCETON_BITS];
    const uint32_t sep = us[2 * PRINCETON_BITS + 1];
    if (sync_hi * 2 < s || sync_hi > s * 2) return false;
    if (sep < 8 * te) return false;

    memset(p, 0, sizeof *p);
    p->proto = OOK_PROTO_PRINCETON;
    p->nbits = PRINCETON_BITS;
    p->te_us = (uint16_t)te;
    uint32_t code = 0;
    for (unsigned b = 0; b < PRINCETON_BITS; b++) {
        const bool one = us[2 * b] > us[2 * b + 1];
        code = (code << 1) | (one ? 1u : 0u);
        set_bit(p->data, b, one);
    }
    p->u.princeton.code = code;
    return true;
}

static size_t decode_princeton(const ook_train_t *t, ook_packet_t *out, size_t cap, size_t n)
{
    /* A frame ends on a low (odd index) after 49 entries starting on a high. */
    for (size_t end = PRINCETON_SPAN - 1; end < t->n; end += 2) {
        ook_packet_t p;
        if (princeton_frame(&t->us[end + 1 - PRINCETON_SPAN], &p)) {
            n = add_packet(out, cap, n, &p);
            end += PRINCETON_SPAN - 2;   /* frames do not overlap */
        }
    }
    return n;
}

/* ---- LaCrosse TX141THBv2 ----------------------------------------------- */

uint8_t ook_lacrosse_digest(const uint8_t *msg, size_t n)
{
    uint8_t sum = 0, key = 0xF4;
    for (size_t k = n; k-- > 0;) {
        for (unsigned i = 0; i < 8; i++) {
            if ((msg[k] >> i) & 1u) sum ^= key;
            const uint8_t shifted = (uint8_t)((unsigned)key << 1);
            key = (key & 0x80u) ? (uint8_t)(shifted ^ 0x31u) : shifted;
        }
    }
    return sum;
}

static bool in_range(uint32_t v, uint32_t lo, uint32_t hi) { return v >= lo && v <= hi; }

static bool is_sync_pair(const uint16_t *us)
{
    return in_range(us[0], LACROSSE_SYNC_MIN_US, LACROSSE_SYNC_MAX_US) &&
           in_range(us[1], LACROSSE_SYNC_MIN_US, LACROSSE_SYNC_MAX_US);
}

/* 40 pairs at us[0..79]. */
static bool lacrosse_frame(const uint16_t *us, ook_packet_t *p)
{
    memset(p, 0, sizeof *p);
    uint32_t short_sum = 0;
    for (unsigned b = 0; b < LACROSSE_BITS; b++) {
        const uint32_t h = us[2 * b], l = us[2 * b + 1];
        const uint32_t mn = h < l ? h : l, mx = h < l ? l : h;
        if (!in_range(h + l, LACROSSE_PERIOD_MIN_US, LACROSSE_PERIOD_MAX_US)) return false;
        if (mn < LACROSSE_PULSE_MIN_US || mx * 10u < mn * 13u) return false;   /* long >= 1.3x short */
        short_sum += mn;
        set_bit(p->data, b, h > l);
    }
    const uint8_t *d = p->data;
    if ((d[0] | d[1] | d[2] | d[3]) == 0) return false;   /* all-zero frame digests to zero */
    if (ook_lacrosse_digest(d, 4) != d[4]) return false;
    p->proto = OOK_PROTO_LACROSSE_TX141THBV2;
    p->nbits = LACROSSE_BITS;
    p->te_us = (uint16_t)(short_sum / LACROSSE_BITS);
    p->u.lacrosse.id = d[0];
    p->u.lacrosse.battery_low = (d[1] & 0x80u) != 0;
    p->u.lacrosse.test = (d[1] & 0x40u) != 0;
    p->u.lacrosse.channel = (uint8_t)((d[1] >> 4) & 0x3u);
    p->u.lacrosse.temp_c10 = (int16_t)((((d[1] & 0x0Fu) << 8) | d[2]) - 500);
    p->u.lacrosse.humidity = d[3];
    return true;
}

static size_t decode_lacrosse(const ook_train_t *t, ook_packet_t *out, size_t cap, size_t n)
{
    size_t i = 0;
    while (i + 1 < t->n) {
        unsigned syncs = 0;
        while (i + 1 < t->n && is_sync_pair(&t->us[i])) { syncs++; i += 2; }
        if (syncs < LACROSSE_SYNC_PAIRS) { i += syncs ? 0 : 2; continue; }
        if (i + 2 * LACROSSE_BITS > t->n) break;
        ook_packet_t p;
        if (lacrosse_frame(&t->us[i], &p)) {
            n = add_packet(out, cap, n, &p);
            i += 2 * LACROSSE_BITS;
        }
    }
    return n;
}

size_t ook_decode(const ook_train_t *train, ook_packet_t *out, size_t cap)
{
    if (!train || !out || cap == 0 || train->n > OOK_TRAIN_MAX) return 0;
    size_t n = decode_lacrosse(train, out, cap, 0);
    return decode_princeton(train, out, cap, n);
}
