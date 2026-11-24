/*
 * ook_test.c — adversarial tests for ook_pulse (bitstream to trains) and
 * ook_decode (Princeton, LaCrosse TX141THBv2). Signals are synthesised at
 * the sample level, distorted the way the CC1101 demodulator distorts them
 * on the bench (highs ~1 sample long, lows ~1 sample short), and fed in
 * random chunk sizes. Build with -fsanitize=address,undefined.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ook_decode.h"
#include "ook_pulse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_pass, g_fail;
#define CHECK(cond, what) do { \
    if (cond) g_pass++; else { g_fail++; printf("  FAIL  %s (line %d)\n", what, __LINE__); } \
} while (0)

#define BAUD 25000u
#define SAMPLE_US 40u
#define GAP_US 6000u
#define GLITCH_US 100u

/* ---- sample-level synthesiser ------------------------------------------ */

#define SYN_MAX_BITS (BAUD * 4)
typedef struct {
    uint8_t bytes[SYN_MAX_BITS / 8];
    size_t bits;
} syn_t;

static uint32_t g_rng = 1;
static uint32_t rnd(void)
{
    g_rng = g_rng * 1103515245u + 12345u;
    return (g_rng >> 8) & 0xFFFFFF;
}

static void syn_init(syn_t *s) { memset(s, 0, sizeof *s); }

static void syn_level(syn_t *s, int level, uint32_t us)
{
    const size_t n = (us + SAMPLE_US / 2) / SAMPLE_US;
    for (size_t i = 0; i < n && s->bits < SYN_MAX_BITS; i++, s->bits++)
        if (level) s->bytes[s->bits / 8] |= (uint8_t)(0x80u >> (s->bits % 8));
}

/* CC1101-like distortion: highs one sample long, lows one sample short. */
static void syn_pair(syn_t *s, uint32_t high_us, uint32_t low_us, int skew)
{
    syn_level(s, 1, high_us + (uint32_t)(skew * (int)SAMPLE_US));
    syn_level(s, 0, low_us - (uint32_t)(skew * (int)SAMPLE_US));
}

static void syn_flip(syn_t *s, size_t bit)
{
    if (bit < s->bits) s->bytes[bit / 8] ^= (uint8_t)(0x80u >> (bit % 8));
}

typedef struct {
    ook_train_t trains[64];
    size_t n;
    ook_packet_t pk[64];
    size_t npk;
} sink_t;

static void on_train(const ook_train_t *t, void *ctx)
{
    sink_t *k = ctx;
    if (k->n < 64) k->trains[k->n++] = *t;
    ook_packet_t got[8];
    const size_t m = ook_decode(t, got, 8);
    for (size_t i = 0; i < m && k->npk < 64; i++) k->pk[k->npk++] = got[i];
}

/* Feed in random chunks of 1..63 bytes (the capture task's chunk sizes). */
static void run(const syn_t *s, sink_t *k, uint32_t seed)
{
    memset(k, 0, sizeof *k);
    ook_pulse_t st;
    ook_pulse_init(&st, BAUD, GAP_US, GLITCH_US);
    g_rng = seed;
    size_t off = 0;
    const size_t total = (s->bits + 7) / 8;
    uint32_t t = 0;
    while (off < total) {
        size_t c = 1 + rnd() % 63;
        if (c > total - off) c = total - off;
        ook_pulse_feed(&st, s->bytes + off, c, t++, on_train, k);
        off += c;
    }
}

/* ---- protocol synthesis ------------------------------------------------ */

static void syn_princeton(syn_t *s, uint32_t code, uint32_t te, int frames, int skew)
{
    for (int f = 0; f < frames; f++) {
        for (int b = 23; b >= 0; b--) {
            if ((code >> b) & 1u) syn_pair(s, 3 * te, te, skew);
            else syn_pair(s, te, 3 * te, skew);
        }
        syn_pair(s, te, 31 * te, skew);
    }
    syn_level(s, 0, 2 * GAP_US);
}

static void lacrosse_bytes(uint8_t d[5], uint8_t id, bool batt, bool test, uint8_t ch, int temp_c10,
                           uint8_t hum)
{
    const uint32_t raw = (uint32_t)(temp_c10 + 500);
    d[0] = id;
    d[1] = (uint8_t)((batt ? 0x80 : 0) | (test ? 0x40 : 0) | ((ch & 3u) << 4) | ((raw >> 8) & 0x0F));
    d[2] = (uint8_t)raw;
    d[3] = hum;
    d[4] = ook_lacrosse_digest(d, 4);
}

static void syn_lacrosse(syn_t *s, const uint8_t d[5], int repeats, int skew)
{
    for (int r = 0; r < repeats; r++) {
        for (int i = 0; i < 4; i++) syn_pair(s, 833, 833, skew);
        for (unsigned b = 0; b < 40; b++) {
            if ((d[b / 8] >> (7 - b % 8)) & 1u) syn_pair(s, 417, 208, skew);
            else syn_pair(s, 208, 417, skew);
        }
        syn_pair(s, 208, 417, skew);   /* trailing pulse, as on the real sensor */
    }
    syn_level(s, 0, 2 * GAP_US);
}

/* ---- tests ------------------------------------------------------------- */

static void pulse_config(void)
{
    ook_pulse_t st;
    CHECK(!ook_pulse_init(&st, 0, GAP_US, GLITCH_US), "baud 0 is refused");
    CHECK(!ook_pulse_init(&st, BAUD, 100, 100), "gap <= glitch is refused");
    CHECK(!ook_pulse_init(&st, BAUD, 70000, 100), "a gap that does not fit a uint16 is refused");
    CHECK(!ook_pulse_init(NULL, BAUD, GAP_US, GLITCH_US), "NULL state is refused");
    CHECK(ook_pulse_init(&st, BAUD, GAP_US, GLITCH_US), "bench configuration is accepted");
    ook_pulse_feed(&st, NULL, 4, 0, on_train, NULL);   /* must not crash */
    CHECK(true, "NULL data is ignored");
}

static void pulse_shapes(void)
{
    syn_t s;
    sink_t k;

    /* Durations survive arbitrary chunking exactly. */
    syn_init(&s);
    syn_level(&s, 0, 400);
    for (int i = 0; i < 10; i++) syn_pair(&s, 200 + 40 * (uint32_t)i, 600, 0);
    syn_level(&s, 0, 3 * GAP_US);
    bool same = true;
    for (uint32_t seed = 1; seed < 40; seed++) {
        run(&s, &k, seed);
        if (k.n != 1 || k.trains[0].n != 20) { same = false; break; }
        for (int i = 0; i < 10; i++)
            if (k.trains[0].us[2 * i] != 200 + 40 * i) same = false;
        if (k.trains[0].us[19] != GAP_US) same = false;
    }
    CHECK(same, "train durations are independent of chunk boundaries");

    /* A train ends as soon as the silence reaches the gap, without a later edge. */
    syn_init(&s);
    for (int i = 0; i < 6; i++) syn_pair(&s, 400, 400, 0);
    syn_level(&s, 0, GAP_US + 2 * SAMPLE_US);
    run(&s, &k, 7);
    CHECK(k.n == 1 && k.trains[0].us[k.trains[0].n - 1] == GAP_US, "silence closes the train at the gap");

    /* One-sample glitches are absorbed; a pulse is not split. */
    syn_init(&s);
    for (int i = 0; i < 6; i++) syn_pair(&s, 800, 800, 0);
    syn_level(&s, 0, 3 * GAP_US);
    syn_flip(&s, 10);         /* dip inside the first high */
    syn_flip(&s, 20 + 10);    /* spike inside the first low */
    run(&s, &k, 3);
    CHECK(k.n == 1 && k.trains[0].n == 12 && k.trains[0].us[0] == 800 && k.trains[0].us[1] == 800,
          "one-sample dips and spikes are absorbed");

    /* A spike out of a silence that already ended a train does not start one. */
    syn_init(&s);
    syn_level(&s, 0, 2 * GAP_US);
    syn_level(&s, 1, SAMPLE_US);
    syn_level(&s, 0, 2000);
    for (int i = 0; i < 6; i++) syn_pair(&s, 400, 400, 0);
    syn_level(&s, 0, 3 * GAP_US);
    run(&s, &k, 5);
    CHECK(k.n == 1 && k.trains[0].us[0] == 400, "a lone spike in silence is not a pulse");

    /* Short trains are noise. */
    syn_init(&s);
    syn_pair(&s, 400, 400, 0);
    syn_pair(&s, 400, 400, 0);
    syn_level(&s, 0, 3 * GAP_US);
    run(&s, &k, 9);
    CHECK(k.n == 0, "trains under OOK_TRAIN_MIN are dropped");

    /* An endless toggle is cut at OOK_TRAIN_MAX and flagged, never overrun. */
    syn_init(&s);
    for (int i = 0; i < OOK_TRAIN_MAX; i++) syn_pair(&s, 200, 200, 0);
    syn_level(&s, 0, 3 * GAP_US);
    run(&s, &k, 11);
    CHECK(k.n == 1 && k.trains[0].truncated && k.trains[0].n == OOK_TRAIN_MAX,
          "an oversized train is truncated and flagged");

    /* reset drops a partial train. */
    ook_pulse_t st;
    ook_pulse_init(&st, BAUD, GAP_US, GLITCH_US);
    memset(&k, 0, sizeof k);
    syn_init(&s);
    for (int i = 0; i < 20; i++) syn_pair(&s, 400, 400, 0);
    ook_pulse_feed(&st, s.bytes, s.bits / 8, 0, on_train, &k);
    ook_pulse_reset(&st);
    const uint8_t zeros[64] = { 0 };
    for (int i = 0; i < 4; i++) ook_pulse_feed(&st, zeros, sizeof zeros, 1, on_train, &k);
    CHECK(k.n == 0, "reset discards the partial train (no train spans an overflow)");
}

static void princeton(void)
{
    syn_t s;
    sink_t k;

    syn_init(&s);
    syn_princeton(&s, 0xA5C3E1, 400, 10, 1);
    run(&s, &k, 21);
    bool all = k.npk == 10;
    for (size_t i = 0; i < k.npk; i++)
        all = all && k.pk[i].proto == OOK_PROTO_PRINCETON && k.pk[i].u.princeton.code == 0xA5C3E1 &&
              k.pk[i].te_us >= 380 && k.pk[i].te_us <= 420;
    CHECK(all, "Flipper-style Princeton A5C3E1 te=400: 10 frames, 10 packets, te measured");
    CHECK(k.npk && k.pk[0].data[0] == 0xA5 && k.pk[0].data[1] == 0xC3 && k.pk[0].data[2] == 0xE1,
          "Princeton frame bits are MSB first");

    /* Short te: sync low (31te) is under the train gap, so frames share one train. */
    syn_init(&s);
    syn_princeton(&s, 0x00F00F, 150, 6, 0);
    run(&s, &k, 22);
    CHECK(k.n == 1 && k.npk == 1 && k.pk[0].u.princeton.code == 0x00F00F && k.pk[0].frames == 6,
          "te=150: six frames in one train merge into one packet with frames=6");

    /* Extreme codes. */
    syn_init(&s);
    syn_princeton(&s, 0x000000, 300, 1, 1);
    syn_princeton(&s, 0xFFFFFF, 300, 1, 1);
    run(&s, &k, 23);
    CHECK(k.npk == 2 && k.pk[0].u.princeton.code == 0 && k.pk[1].u.princeton.code == 0xFFFFFF,
          "all-zero and all-one codes decode");

    /* A missing bit (23 bits) is not a frame. */
    syn_init(&s);
    for (int b = 22; b >= 0; b--) syn_pair(&s, 400, 1200, 0);
    syn_pair(&s, 400, 31 * 400, 0);
    syn_level(&s, 0, 2 * GAP_US);
    run(&s, &k, 24);
    CHECK(k.npk == 0, "a 23-bit frame is rejected");

    /* A sync pulse far longer than te is not a Princeton sync. */
    syn_init(&s);
    for (int b = 0; b < 24; b++) syn_pair(&s, 400, 1200, 0);
    syn_pair(&s, 2000, 31 * 400, 0);
    syn_level(&s, 0, 2 * GAP_US);
    run(&s, &k, 27);
    CHECK(k.n == 1 && k.npk == 0, "a 5te sync pulse is rejected");

    /* Ratio 1.5 (not PWM 1:3) is rejected. */
    syn_init(&s);
    for (int b = 0; b < 24; b++) syn_pair(&s, (b & 1) ? 600 : 400, (b & 1) ? 400 : 600, 0);
    syn_pair(&s, 400, 31 * 400, 0);
    syn_level(&s, 0, 2 * GAP_US);
    run(&s, &k, 25);
    CHECK(k.npk == 0, "a 1:1.5 pulse ratio is not Princeton");

    /* No sync pulse: the 24th bit's low runs straight into silence. */
    syn_init(&s);
    for (int b = 0; b < 23; b++) syn_pair(&s, 400, 1200, 0);
    syn_level(&s, 1, 400);
    syn_level(&s, 0, 2 * GAP_US);
    run(&s, &k, 26);
    CHECK(k.n == 1 && k.npk == 0, "a frame without its sync pulse is rejected");
}

static void lacrosse(void)
{
    syn_t s;
    sink_t k;
    uint8_t d[5];

    /* Bench vector: rtl_433 read id 192, battery low, ch 0, 9.3 C, 56 %. */
    const uint8_t bench[4] = { 0xC0, 0x82, 0x51, 0x38 };
    CHECK(ook_lacrosse_digest(bench, 4) == 0x11, "digest matches the bench frame C0 82 51 38 -> 11");

    lacrosse_bytes(d, 0xC0, true, false, 0, 93, 56);
    CHECK(memcmp(d, (const uint8_t[]){ 0xC0, 0x82, 0x51, 0x38, 0x11 }, 5) == 0,
          "field packing reproduces the bench frame");
    syn_init(&s);
    syn_lacrosse(&s, d, 4, 1);
    run(&s, &k, 31);
    CHECK(k.npk == 1 && k.pk[0].proto == OOK_PROTO_LACROSSE_TX141THBV2 && k.pk[0].frames == 4,
          "four repeats in one train merge into one packet");
    CHECK(k.npk == 1 && k.pk[0].u.lacrosse.id == 192 && k.pk[0].u.lacrosse.battery_low &&
          !k.pk[0].u.lacrosse.test && k.pk[0].u.lacrosse.channel == 0 &&
          k.pk[0].u.lacrosse.temp_c10 == 93 && k.pk[0].u.lacrosse.humidity == 56,
          "fields: id 192, battery low, ch 0, 9.3 C, 56 %");

    /* Negative temperature, channel, test bit. */
    lacrosse_bytes(d, 0x07, false, true, 2, -123, 99);
    syn_init(&s);
    syn_lacrosse(&s, d, 3, -1);
    run(&s, &k, 32);
    CHECK(k.npk == 1 && k.pk[0].u.lacrosse.temp_c10 == -123 && k.pk[0].u.lacrosse.channel == 2 &&
          k.pk[0].u.lacrosse.test && !k.pk[0].u.lacrosse.battery_low && k.pk[0].u.lacrosse.humidity == 99,
          "-12.3 C, channel 2, test bit, opposite skew");

    /* Every single-bit corruption is caught by the digest. */
    lacrosse_bytes(d, 0xC0, true, false, 0, 93, 56);
    int caught = 0;
    for (unsigned b = 0; b < 40; b++) {
        uint8_t e[5];
        memcpy(e, d, 5);
        e[b / 8] ^= (uint8_t)(0x80u >> (b % 8));
        syn_init(&s);
        syn_lacrosse(&s, e, 1, 1);
        run(&s, &k, 40 + b);
        caught += k.npk == 0;
    }
    CHECK(caught == 40, "all 40 single-bit corruptions are rejected");

    /* A clipped frame (first sync pairs lost) with only 2 sync pairs is rejected. */
    syn_init(&s);
    for (int i = 0; i < 2; i++) syn_pair(&s, 833, 833, 0);
    for (unsigned b = 0; b < 40; b++) {
        if ((d[b / 8] >> (7 - b % 8)) & 1u) syn_pair(&s, 417, 208, 0);
        else syn_pair(&s, 208, 417, 0);
    }
    syn_level(&s, 0, 2 * GAP_US);
    run(&s, &k, 33);
    CHECK(k.npk == 0, "fewer than three sync pairs is not a frame start");

    /* Truncated frame at the end of a train: no read past n (ASan). */
    syn_init(&s);
    for (int i = 0; i < 4; i++) syn_pair(&s, 833, 833, 0);
    for (unsigned b = 0; b < 30; b++) syn_pair(&s, 417, 208, 0);
    syn_level(&s, 0, 2 * GAP_US);
    run(&s, &k, 34);
    CHECK(k.npk == 0, "a frame cut after 30 bits is rejected");

    /* Stale entries past n are never read: n cuts a valid frame whose rest is
     * still in the array (a reused train buffer). */
    {
        syn_init(&s);
        syn_lacrosse(&s, d, 1, 0);
        run(&s, &k, 36);
        CHECK(k.n == 1 && k.npk == 1, "reference frame decodes");
        static ook_train_t cut;
        cut = k.trains[0];
        cut.n = 8 + 2 * 30;   /* syncs + 30 bits; bits 30..39 remain in us[] */
        ook_packet_t o[2];
        CHECK(ook_decode(&cut, o, 2) == 0, "a frame cut by n is rejected even with valid stale data beyond n");
        /* Same at the very end of a full train: reading on would leave the array (ASan/UBSan). */
        static ook_train_t edge;
        memset(&edge, 0, sizeof edge);
        edge.n = OOK_TRAIN_MAX;
        for (unsigned i = 0; i < OOK_TRAIN_MAX; i++) edge.us[i] = (i & 1) ? 417 : 208;
        for (unsigned i = OOK_TRAIN_MAX - 12; i < OOK_TRAIN_MAX - 4; i++) edge.us[i] = 833;
        CHECK(ook_decode(&edge, o, 2) == 0, "sync pairs at the end of a full train do not read past it");
    }

    /* An all-zero frame digests to zero; it is refused. */
    const uint8_t z[4] = { 0 };
    CHECK(ook_lacrosse_digest(z, 4) == 0, "all-zero digest is zero (why it is refused)");
    syn_init(&s);
    syn_lacrosse(&s, (const uint8_t[]){ 0, 0, 0, 0, 0 }, 2, 0);
    run(&s, &k, 35);
    CHECK(k.npk == 0, "an all-zero LaCrosse frame is refused");
}

static void noise(void)
{
    /* Random sample noise at several densities: nothing decodes. */
    static syn_t s;
    sink_t k;
    size_t trains = 0, packets = 0;
    for (int density = 1; density <= 5; density++) {
        for (uint32_t seed = 100; seed < 110; seed++) {
            syn_init(&s);
            g_rng = seed * 7919u + (uint32_t)density;
            for (size_t i = 0; i < SYN_MAX_BITS; i++)
                if (rnd() % 10 < (uint32_t)density) s.bytes[i / 8] |= (uint8_t)(0x80u >> (i % 8));
            s.bits = SYN_MAX_BITS;
            run(&s, &k, seed);
            trains += k.n;
            packets += k.npk;
        }
    }
    CHECK(packets == 0, "200 s of random sample noise decodes to nothing");
    CHECK(trains > 0, "the noise did produce trains for the decoders to reject");

    /* Random-duration hostile trains straight into the decoder. */
    static ook_train_t t;
    ook_packet_t out[2];
    bool bounded = true;
    for (uint32_t seed = 1; seed < 2000; seed++) {
        g_rng = seed;
        t.n = (uint16_t)(rnd() % (OOK_TRAIN_MAX + 1));
        for (unsigned i = 0; i < t.n; i++) t.us[i] = (uint16_t)(rnd() % 1500);
        if (ook_decode(&t, out, 2) > 2) bounded = false;
    }
    CHECK(bounded, "hostile trains never exceed the output capacity");
    t.n = OOK_TRAIN_MAX + 1;
    CHECK(ook_decode(&t, out, 2) == 0, "a train claiming n > OOK_TRAIN_MAX is refused");
    CHECK(ook_decode(NULL, out, 2) == 0 && ook_decode(&t, NULL, 2) == 0, "NULL inputs are refused");
}

static void capacity(void)
{
    /* Three distinct Princeton frames in one train, cap 2: the first two kept. */
    syn_t s;
    syn_init(&s);
    syn_princeton(&s, 0x111111, 150, 1, 0);
    s.bits -= (2 * GAP_US) / SAMPLE_US;   /* keep frames in one train */
    syn_princeton(&s, 0x222222, 150, 1, 0);
    s.bits -= (2 * GAP_US) / SAMPLE_US;
    syn_princeton(&s, 0x333333, 150, 1, 0);
    ook_pulse_t st;
    ook_pulse_init(&st, BAUD, GAP_US, GLITCH_US);
    sink_t k;
    memset(&k, 0, sizeof k);
    ook_pulse_feed(&st, s.bytes, (s.bits + 7) / 8, 0, on_train, &k);
    CHECK(k.n == 1 && k.npk == 3, "three distinct frames in one train decode to three packets");
    ook_packet_t two[2];
    CHECK(k.n == 1 && ook_decode(&k.trains[0], two, 2) == 2 && two[0].u.princeton.code == 0x111111 &&
          two[1].u.princeton.code == 0x222222, "cap 2 keeps the first two in order");

    ook_packet_t a = k.pk[0], b = k.pk[0];
    CHECK(ook_packet_same(&a, &b), "a packet equals itself");
    b.data[2] ^= 1;
    CHECK(!ook_packet_same(&a, &b), "one changed bit is a different packet");
    CHECK(!ook_packet_same(&a, NULL), "NULL is never the same");
    CHECK(!strcmp(ook_proto_name(OOK_PROTO_LACROSSE_TX141THBV2), "lacrosse_tx141thbv2") &&
          !strcmp(ook_proto_name((ook_proto_t)99), "none"), "protocol names");
}

int main(void)
{
    pulse_config();
    pulse_shapes();
    princeton();
    lacrosse();
    noise();
    capacity();
    printf("ook_test: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
