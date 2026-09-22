/*
 * cc1101_radio.h — CC1101 driver layer, RX path only
 * (c5-dual-radio-wiring.md §2, §4, §5).
 *
 * Shares the SPI2 bus (SCK/MISO/MOSI) that lora_radio.c already initializes;
 * this file only adds its own device and drives its own CSn manually, same
 * shape as lora_radio.c's NSS handling. legacy_recon.c is the survey/OCP
 * layer on top of this; this file knows nothing about OCP.
 *
 * No transmit-shaped function exists here, and never will (D-8): the STX and
 * SFSTXON strobes are never issued.
 *
 * Pins, wiring-doc §2/§4 (not a stacked shield — AGENTS.md gotcha 16 applies
 * here too, this is a point-to-point harness):
 *   SCK=GPIO8  MISO=GPIO9  MOSI=GPIO10  CSn=GPIO7 (D3)
 *   GDO0/GDO2 unconnected in this harness revision — RX is polling, not
 *   IRQ-driven (wiring doc §4.1).
 *
 * The 26 MHz crystal assumption (FXOSC_HZ below) is a bench-verified
 * starting point, not a datasheet-confirmed constant for the delivered
 * E07-M1101D-SMA module — same epistemic stance as lora_radio.h's D-10 TCXO
 * delay. Confirm against the physical module if frequency accuracy matters.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CC1101_RADIO_H
#define OSCILLA_CC1101_RADIO_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct {
    uint32_t freq_hz;   /* Caller's choice; no default (receive-only, no region plan). */
} cc1101_rx_params_t;

/* One FIFO drain chunk. 64 bytes is the chip's own RX FIFO depth
 * (datasheet §8) — a single drain can never exceed it. */
#define CC1101_MAX_CHUNK 64

typedef struct {
    uint8_t len;
    uint8_t payload[CC1101_MAX_CHUNK];
    int16_t rssi_dbm;
} cc1101_chunk_t;

typedef enum {
    CC1101_EVT_CHUNK,     /* chunk field below is valid */
    CC1101_EVT_OVERFLOW,  /* RX FIFO overflowed between polls; FIFO was flushed */
} cc1101_evt_kind_t;

typedef struct {
    cc1101_evt_kind_t kind;
    cc1101_chunk_t    chunk;   /* only when kind == CC1101_EVT_CHUNK */
} cc1101_event_t;

/* SPI device + GPIO bring-up. Call once at boot, after lora_radio_init() has
 * already initialized the shared SPI2 bus. Idempotent-safe to call even if
 * the CC1101 harness isn't attached; failures here stay local (main.c's "a
 * radio that fails to come up stays local" pattern). */
esp_err_t cc1101_radio_init(void);

/* Full bring-up (reset, PARTNUM/VERSION probe, modem config) and enter
 * continuous RX with the given params. Fails closed: on any error the chip
 * is left idle, never mid-configured. */
esp_err_t cc1101_radio_rx_start(const cc1101_rx_params_t *params);

/* Idle (SIDLE strobe). Safe to call whether or not RX is running. */
void cc1101_radio_rx_stop(void);

bool cc1101_radio_is_running(void);

/* Blocking pop from the polling task's output queue. Returns false on
 * timeout, never blocks forever regardless of timeout_ms value chosen by
 * the caller. */
bool cc1101_radio_next_event(cc1101_event_t *out, uint32_t timeout_ms);

/* Resets the chip and reads PARTNUM/VERSION — the concrete "is the chip
 * alive" bench check (datasheet Table 45). Leaves the chip idle either way. */
esp_err_t cc1101_radio_read_id(uint8_t *partnum, uint8_t *version);

#endif /* OSCILLA_CC1101_RADIO_H */
