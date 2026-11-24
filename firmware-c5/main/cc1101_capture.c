/*
 * cc1101_capture.c — see cc1101_capture.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_capture.h"

#include <stddef.h>
#include <string.h>

#include "cc1101_rx.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/message_buffer.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define CHUNK_HEADER offsetof(cc1101_chunk_t, data)

_Static_assert(CC1101_CAPTURE_TASK_PRIO < 10, "capture would outrank OCP dispatch (DESIGN 6.4)");
_Static_assert(CC1101_FIFO_SIZE <= UINT8_MAX, "chunk length is a uint8_t");

static const cc1101_bus_t *s_bus;
static TaskHandle_t s_task;
static SemaphoreHandle_t s_chip;          /* held for each drain and across a pause */
static MessageBufferHandle_t s_chunks;
static volatile bool s_running;
static volatile bool s_resync;            /* restart the tick after start/resume */
static volatile uint32_t s_period_ms = CC1101_CAPTURE_PERIOD_MS_DEFAULT;
static cc1101_capture_stats_t s_stats;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void drain_once(void)
{
    cc1101_chunk_t c;
    size_t n = 0;
    bool ovf = false;
    const cc1101_err_t err = cc1101_rx_drain(s_bus, c.data, sizeof c.data, &n, &ovf);
    if (err != CC1101_OK) { s_stats.errors++; return; }
    if (ovf) s_stats.overflows++;
    if (!n) return;
    c.t_ms = now_ms();
    c.n = (uint8_t)n;
    if (cc1101_rx_rssi(s_bus, &c.rssi_dbm) != CC1101_OK) c.rssi_dbm = INT16_MIN;
    if (xMessageBufferSend(s_chunks, &c, CHUNK_HEADER + n, 0) == 0) s_stats.dropped++;
    else s_stats.bytes += n;
}

static void capture_task(void *arg)
{
    (void)arg;
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        if (!s_running) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);   /* parked: no CPU, no bus */
            s_resync = true;
            continue;
        }
        const TickType_t period = pdMS_TO_TICKS(s_period_ms) ? pdMS_TO_TICKS(s_period_ms) : 1;
        if (s_resync) {
            s_resync = false;
            wake = xTaskGetTickCount();
        }
        /* xTaskDelayUntil returns pdFALSE when the deadline had already passed. */
        if (xTaskDelayUntil(&wake, period) == pdFALSE) {
            s_stats.late++;
            wake = xTaskGetTickCount();   /* do not burst to catch up */
        }
        if (!s_running) continue;
        xSemaphoreTake(s_chip, portMAX_DELAY);
        if (s_running) drain_once();
        xSemaphoreGive(s_chip);
    }
}

esp_err_t cc1101_capture_init(const cc1101_bus_t *bus)
{
    if (!bus) return ESP_ERR_INVALID_ARG;
    if (s_task) return ESP_OK;
    s_bus = bus;
    s_chip = xSemaphoreCreateMutex();
    /* 25 kbaud is ~3 KB/s, so PSRAM is plenty fast and keeps 4 KB of
     * internal RAM for the radio stacks. Only tasks touch it, never an ISR. */
    s_chunks = xMessageBufferCreateWithCaps(CC1101_CAPTURE_BUFFER_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_chunks) s_chunks = xMessageBufferCreate(CC1101_CAPTURE_BUFFER_BYTES);
    if (!s_chip || !s_chunks) return ESP_ERR_NO_MEM;
    if (xTaskCreate(capture_task, "cc1101_cap", 2048, NULL, CC1101_CAPTURE_TASK_PRIO, &s_task) !=
        pdPASS) {
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void discard_chunks(void)
{
    cc1101_chunk_t c;
    while (xMessageBufferReceive(s_chunks, &c, sizeof c, 0) > 0) {
    }
}

esp_err_t cc1101_capture_start(uint32_t period_ms)
{
    if (!s_task) return ESP_ERR_INVALID_STATE;
    if (period_ms < 1 || period_ms > CC1101_CAPTURE_PERIOD_MS_MAX) return ESP_ERR_INVALID_ARG;
    cc1101_capture_stop();
    /* Start from an empty FIFO: samples (or an overflow) left from before
     * the start would be misdated or miscounted. */
    xSemaphoreTake(s_chip, portMAX_DELAY);
    const cc1101_err_t err = cc1101_rx_start(s_bus, NULL);
    xSemaphoreGive(s_chip);
    if (err != CC1101_OK) return ESP_FAIL;
    s_period_ms = period_ms;
    memset(&s_stats, 0, sizeof s_stats);
    s_resync = true;
    s_running = true;
    xTaskNotifyGive(s_task);
    return ESP_OK;
}

void cc1101_capture_stop(void)
{
    if (!s_task) return;
    s_running = false;
    xSemaphoreTake(s_chip, portMAX_DELAY);   /* wait out a drain in flight */
    xSemaphoreGive(s_chip);
    discard_chunks();
}

void cc1101_capture_pause(void)
{
    if (s_task) xSemaphoreTake(s_chip, portMAX_DELAY);
}

void cc1101_capture_resume(void)
{
    if (!s_task) return;
    s_resync = true;
    xSemaphoreGive(s_chip);
}

bool cc1101_capture_running(void) { return s_running; }

bool cc1101_capture_read(cc1101_chunk_t *out, uint32_t timeout_ms)
{
    if (!s_task || !out) return false;
    const size_t got = xMessageBufferReceive(s_chunks, out, sizeof *out, pdMS_TO_TICKS(timeout_ms));
    if (got <= CHUNK_HEADER) return false;
    out->n = (uint8_t)(got - CHUNK_HEADER);
    return true;
}

void cc1101_capture_get_stats(cc1101_capture_stats_t *out)
{
    if (out) *out = s_stats;
}
