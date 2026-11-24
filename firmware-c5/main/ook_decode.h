/*
 * ook_decode.h — OOK pulse-train decoders (DESIGN §6.1). Receive-only.
 *
 * Input is an ook_pulse train (high first, alternating, microseconds).
 * Supported, from the bench captures recorded in WORKLOG 2026-11-23:
 *   - Princeton / EV1527 fixed code: 24 bits, bit = (1te,3te) for 0 and
 *     (3te,1te) for 1, each frame followed by a sync of 1te high and >=8te
 *     low. te 80..700 us.
 *   - LaCrosse TX141THBv2 temperature/humidity: >=3 sync pairs of ~0.8 ms,
 *     then 40 PWM bits (long high = 1, period ~0.7 ms): id, battery-low,
 *     test, channel, 12-bit temperature (raw - 500 = 0.1 C), humidity,
 *     and an 8-bit LFSR digest over the first 4 bytes.
 * Pure C, no allocation, host-tested (firmware-c5/test/host/ook_test.c).
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_OOK_DECODE_H
#define OSCILLA_OOK_DECODE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ook_pulse.h"

typedef enum {
    OOK_PROTO_NONE = 0,
    OOK_PROTO_PRINCETON = 1,
    OOK_PROTO_LACROSSE_TX141THBV2 = 2,
} ook_proto_t;

typedef struct {
    ook_proto_t proto;
    uint8_t nbits;
    uint8_t data[5];        /* frame bits, MSB first */
    uint16_t te_us;         /* measured base pulse (Princeton) or short pulse (LaCrosse) */
    uint8_t frames;         /* identical frames seen in this train */
    union {
        struct {
            uint32_t code;  /* 24-bit fixed code */
        } princeton;
        struct {
            uint8_t id;
            uint8_t channel;
            bool battery_low;
            bool test;
            int16_t temp_c10;   /* tenths of a degree C */
            uint8_t humidity;   /* %RH as sent; not range-checked */
        } lacrosse;
    } u;
} ook_packet_t;

/* Decode every distinct frame in one train into out[0..cap). Returns the
 * number of packets written. Identical frames are merged (frames counts
 * them). Never reads past train->n. */
size_t ook_decode(const ook_train_t *train, ook_packet_t *out, size_t cap);

/* Same protocol and frame bits. */
bool ook_packet_same(const ook_packet_t *a, const ook_packet_t *b);

const char *ook_proto_name(ook_proto_t proto);

/* TX141THBv2 digest: reflected 8-bit LFSR, generator 0x31, key 0xF4. */
uint8_t ook_lacrosse_digest(const uint8_t *msg, size_t n);

#endif /* OSCILLA_OOK_DECODE_H */
