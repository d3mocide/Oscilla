/*
 * lora_radio.h — SX1262 driver layer, RX path only (DESIGN §6.1).
 *
 * Owns the Wio-SX1262's SPI bus, reset, TCXO/regulator bring-up, and the
 * DIO1 ISR -> radio task handoff. lora_recon.c (P3, not yet written) is the
 * survey/framing/OCP layer on top of this; this file knows nothing about
 * OCP and has no verb-shaped entry point.
 *
 * No transmit-shaped function exists here, and never will (D-8). Every
 * sequence below only ever asks the chip to receive.
 *
 * Pins are Rev D's, wired point-to-point, not a stacked shield
 * (AGENTS.md gotcha 16 — do not re-derive these from the Wio board's own
 * D0..D6 passthrough labels):
 *   SCK=GPIO8  MISO=GPIO9  MOSI=GPIO10  NSS=GPIO23
 *   RST=GPIO1  DIO1=GPIO0  BUSY=GPIO24  RF_SW=GPIO25
 *
 * TCXO voltage/delay is D-10: tcxoVoltage=0x02 (1.8 V), delay=640 (10 ms) as
 * a bench-verified starting point, not an asserted-correct constant — see
 * docs/DECISIONS.md D-10.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_LORA_RADIO_H
#define OSCILLA_LORA_RADIO_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* LoRa bandwidth codes, SX1262 datasheet Table 13-48. Not sequential with
 * kHz value — do not substitute a raw kHz number here. */
typedef enum {
    LORA_BW_7_8   = 0x00,
    LORA_BW_10_4  = 0x08,
    LORA_BW_15_6  = 0x01,
    LORA_BW_20_8  = 0x09,
    LORA_BW_31_25 = 0x02,
    LORA_BW_41_7  = 0x0A,
    LORA_BW_62_5  = 0x03,
    LORA_BW_125   = 0x04,
    LORA_BW_250   = 0x05,
    LORA_BW_500   = 0x06,
} lora_bw_t;

typedef struct {
    uint32_t  freq_hz;   /* Caller's choice; no default (receive-only, no region plan — D-9). */
    uint8_t   sf;        /* 5..12 */
    lora_bw_t bw;
    uint8_t   cr;        /* 1..4 -> 4/5 .. 4/8 */
    uint8_t   sync_word; /* LoRa sync word byte (e.g. 0x12 private, 0x2B Meshtastic) — a receive
                           * filter, not cosmetic: the chip only raises RX for a matching sync
                           * word (see lora_set_sync_word() in lora_radio.c). */
} lora_rx_params_t;

#define LORA_MAX_PAYLOAD 255

typedef struct {
    uint8_t len;
    uint8_t payload[LORA_MAX_PAYLOAD];
    int16_t rssi_dbm;    /* GetPacketStatus RssiPkt / -2 */
    float   snr_db;      /* GetPacketStatus SnrPkt / 4, signed */
} lora_packet_t;

typedef enum {
    LORA_EVT_PACKET,      /* packet field below is valid */
    LORA_EVT_TIMEOUT,
    LORA_EVT_CRC_ERR,
    LORA_EVT_HEADER_ERR,
} lora_evt_kind_t;

typedef struct {
    lora_evt_kind_t kind;
    lora_packet_t   packet;   /* only when kind == LORA_EVT_PACKET */
} lora_event_t;

/* Monotonic within one lora_radio_rx_start() session. These identify queue
 * boundaries; they are not an RF-layer traffic estimate. */
typedef struct {
    uint32_t rx;
    uint32_t crc_err;
    uint32_t header_err;
    uint32_t irq_drop;
    uint32_t event_drop;
    uint32_t hw_fault;   /* GetIrqStatus/ClearIrqStatus failed, or read back empty, while running */
} lora_radio_stats_t;

/* SPI bus + GPIO bring-up. Call once at boot. Idempotent-safe to call even
 * if the Wio harness isn't attached; failures here stay local (main.c's
 * "a radio that fails to come up stays local" pattern). */
esp_err_t lora_radio_init(void);

/* Full bring-up (reset, TCXO, regulator, RF-switch, modem config) and enter
 * continuous RX with the given params. Fails closed: on any error the chip
 * is left in standby with RF_SW low and IRQs masked, never mid-configured. */
esp_err_t lora_radio_rx_start(const lora_rx_params_t *params);

/* Standby + RF_SW low + IRQs masked. Safe to call whether or not RX is
 * running — this is the arbiter teardown hook shape. */
void lora_radio_rx_stop(void);

bool lora_radio_is_running(void);

/* Blocking pop from the radio task's output queue. Returns false on
 * timeout, never blocks forever regardless of timeout_ms value chosen by
 * the caller. */
bool lora_radio_next_event(lora_event_t *out, uint32_t timeout_ms);

/* Snapshot the current listener-session counters. Safe while RX is active;
 * a concurrent event may appear in either adjacent snapshot. */
void lora_radio_get_stats(lora_radio_stats_t *out);

#endif /* OSCILLA_LORA_RADIO_H */
