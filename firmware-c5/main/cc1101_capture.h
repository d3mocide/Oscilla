/*
 * cc1101_capture.h — fixed-tick CC1101 RX FIFO drain task (DESIGN §6.1).
 *
 * In FIFO sampling mode (cc1101_profile_make_fifo) the 64-byte RX FIFO lasts
 * ~20 ms at 25 kbaud. Draining it from a console or engine loop lost ~1
 * chunk per 20 s (WORKLOG 2026-11-23), so draining runs in its own task on a
 * vTaskDelayUntil tick, above engine priority and below OCP dispatch
 * (DESIGN §6.4). Chunks go to a message buffer; a slow consumer loses chunks
 * there (counted as `dropped`), never inside the chip.
 *
 * The task only reads the FIFO (and restarts RX after an overflow). Any other
 * multi-step sequence on the chip — apply profile, reset, stress — must run
 * between cc1101_capture_pause() and cc1101_capture_resume(). Receive-only.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_CC1101_CAPTURE_H
#define OSCILLA_CC1101_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cc1101_bus.h"
#include "esp_err.h"

/* Below OCP dispatch (10) so `stop` still lands; above engines (<= 5). */
#define CC1101_CAPTURE_TASK_PRIO 6
#define CC1101_CAPTURE_PERIOD_MS_DEFAULT 8u
#define CC1101_CAPTURE_PERIOD_MS_MAX 40u   /* bench: >20 ms must overflow at 25 kbaud */
#define CC1101_CAPTURE_BUFFER_BYTES 4096u

typedef struct {
    uint32_t t_ms;                       /* esp_timer ms when the chunk was read */
    int16_t rssi_dbm;                    /* RSSI read right after the drain */
    uint8_t n;                           /* 1..CC1101_FIFO_SIZE */
    uint8_t data[CC1101_FIFO_SIZE];
} cc1101_chunk_t;

typedef struct {
    uint32_t bytes;       /* delivered to the message buffer */
    uint32_t overflows;   /* RX FIFO overflowed in the chip (samples lost) */
    uint32_t dropped;     /* chunks lost because the consumer was slow */
    uint32_t errors;      /* drain transport/state errors */
    uint32_t late;        /* ticks that started > one period late */
} cc1101_capture_stats_t;

/* Create the task (parked). The bus must outlive the task. */
esp_err_t cc1101_capture_init(const cc1101_bus_t *bus);

/* Restart RX with an empty FIFO, then drain every period_ms
 * (1..CC1101_CAPTURE_PERIOD_MS_MAX) with fresh stats. The chip must already
 * hold a FIFO profile. ESP_FAIL when the RX restart fails (capture stays
 * stopped). */
esp_err_t cc1101_capture_start(uint32_t period_ms);

/* Stop draining and discard buffered chunks. Returns once no drain is in
 * flight. Safe when already stopped. */
void cc1101_capture_stop(void);

/* Hold the task off the chip without losing state; returns once no drain is
 * in flight. Each pause must be paired with one resume from the same task;
 * they do not nest. Harmless when stopped. */
void cc1101_capture_pause(void);
void cc1101_capture_resume(void);

bool cc1101_capture_running(void);

/* Next chunk, waiting up to timeout_ms. false when none arrived. */
bool cc1101_capture_read(cc1101_chunk_t *out, uint32_t timeout_ms);

void cc1101_capture_get_stats(cc1101_capture_stats_t *out);

#endif /* OSCILLA_CC1101_CAPTURE_H */
