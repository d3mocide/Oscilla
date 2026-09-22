/*
 * legacy_recon.c — see legacy_recon.h.
 *
 * LoRa and the CC1101 share one SPI bus and may not receive at the same
 * time (c5-dual-radio-wiring.md §5.2) — both go through subghz_arbiter so a
 * switch from one to the other always tears the first one down cleanly.
 *
 * SPDX-License-Identifier: MIT
 */

#include "legacy_recon.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "cc1101_radio.h"
#include "ocp.h"
#include "ocp_frame.h"
#include "ocp_parse.h"
#include "subghz_arbiter.h"

static const char *TAG = "legacy_recon";

static bool s_ready;
static bool s_configured;
static cc1101_rx_params_t s_params;

esp_err_t legacy_recon_init(void)
{
    esp_err_t err = cc1101_radio_init();
    s_ready = (err == ESP_OK);
    if (err != ESP_OK) ESP_LOGE(TAG, "cc1101_radio_init: %s", esp_err_to_name(err));
    return err;
}

bool legacy_recon_ready(void) { return s_ready; }

/* legacy_config <freq_hz>. No default (D-9's reasoning applies here too:
 * receive-only, no region plan — the caller always states one). */
void legacy_cmd_config(int argc, char **argv)
{
    (void)argc;   /* dispatch() already enforced exactly 1 arg */
    uint32_t freq = 0;
    if (!ocp_parse_u32(argv[1], &freq)) {
        ocp_emit_error(OCP_ERR_BADARG, "freq must be a whole unsigned integer");
        return;
    }
    /* E07-M1101D-SMA band, wiring-doc §1: 387-464 MHz. */
    if (freq < 387000000UL || freq > 464000000UL) {
        ocp_emit_error(OCP_ERR_BADARG, "freq out of E07-M1101D-SMA range");
        return;
    }

    s_params.freq_hz = freq;
    s_configured = true;

    ocp_emit_compact(OCP_MARK_CFG, "%s=%lu", OCP_K_FREQ, (unsigned long)freq);
}

static void emit_chunk(const cc1101_chunk_t *c)
{
    ocp_event_record_t event = { .kind = OCP_EVENT_RECORD_LEGACY };
    event.data.legacy.len = c->len;
    memcpy(event.data.legacy.payload, c->payload, c->len);
    event.data.legacy.rssi_dbm = c->rssi_dbm;
    (void)ocp_event_submit(&event);
}

/* Drains cc1101_radio's event queue until it stops, then exits itself — same
 * shape as lora_recon.c's drain_task(). */
static void drain_task(void *arg)
{
    (void)arg;
    cc1101_event_t evt;
    while (cc1101_radio_is_running()) {
        if (!cc1101_radio_next_event(&evt, 200)) continue;
        switch (evt.kind) {
            case CC1101_EVT_CHUNK:    emit_chunk(&evt.chunk); break;
            case CC1101_EVT_OVERFLOW: break;   /* FIFO already flushed by the poll task */
        }
    }
    vTaskDelete(NULL);
}

static void legacy_teardown(void)
{
    cc1101_radio_rx_stop();
    subghz_arbiter_release(SUBGHZ_OWNER_CC1101);
}

void legacy_cmd_listen(void)
{
    if (!s_configured) {
        ocp_emit_error(OCP_ERR_BADARG, "legacy_config required first");
        return;
    }
    if (cc1101_radio_is_running()) {
        ocp_emit_error(OCP_ERR_BUSY, "already listening");
        return;
    }
    if (subghz_arbiter_acquire(SUBGHZ_OWNER_CC1101, legacy_teardown) != ESP_OK) {
        ocp_emit_error(OCP_ERR_BUSY, "lora is using the shared sub-GHz bus");
        return;
    }

    esp_err_t err = cc1101_radio_rx_start(&s_params);
    if (err != ESP_OK) {
        subghz_arbiter_release(SUBGHZ_OWNER_CC1101);
        ocp_emit_error(OCP_ERR_HWFAULT, esp_err_to_name(err));
        return;
    }

    if (xTaskCreate(drain_task, "legacy_drain", 4096, NULL, 5, NULL) != pdPASS) {
        legacy_teardown();
        ocp_emit_error(OCP_ERR_INTERNAL, "could not start drain task");
        return;
    }

    ocp_emit_compact(OCP_MARK_LEGACY, "%s=%lu", OCP_K_FREQ, (unsigned long)s_params.freq_hz);
}

void legacy_cmd_status(void)
{
    bool running = cc1101_radio_is_running();

    /* PARTNUM/VERSION is the concrete hardware-alive check (see plan). Only
     * probed while idle: it resets the chip, which would otherwise silently
     * kill an in-progress capture without clearing s_running. */
    uint8_t partnum = 0xFF, version = 0xFF;
    bool have_id = false;
    if (!running) {
        have_id = (cc1101_radio_read_id(&partnum, &version) == ESP_OK);
    }

    if (!s_configured) {
        if (have_id) {
            ocp_emit_compact(OCP_MARK_LEGACY, "%s=%d configured=0 %s=%u %s=%u",
                             OCP_K_RUNNING, running ? 1 : 0,
                             OCP_K_PARTNUM, (unsigned)partnum, OCP_K_CHIPVER, (unsigned)version);
        } else {
            ocp_emit_compact(OCP_MARK_LEGACY, "%s=%d configured=0", OCP_K_RUNNING, running ? 1 : 0);
        }
        return;
    }
    if (have_id) {
        ocp_emit_compact(OCP_MARK_LEGACY, "%s=%d %s=%lu %s=%u %s=%u",
                         OCP_K_RUNNING, running ? 1 : 0,
                         OCP_K_FREQ, (unsigned long)s_params.freq_hz,
                         OCP_K_PARTNUM, (unsigned)partnum, OCP_K_CHIPVER, (unsigned)version);
    } else {
        ocp_emit_compact(OCP_MARK_LEGACY, "%s=%d %s=%lu",
                         OCP_K_RUNNING, running ? 1 : 0,
                         OCP_K_FREQ, (unsigned long)s_params.freq_hz);
    }
}

bool legacy_cmd_stop(void)
{
    if (subghz_arbiter_owner() != SUBGHZ_OWNER_CC1101) return false;
    return subghz_arbiter_stop();
}
