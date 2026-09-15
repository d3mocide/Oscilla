/*
 * lora_recon.c — see lora_recon.h.
 *
 * Owns nothing electrical: all SPI/GPIO work lives in lora_radio.c. This
 * file only translates OCP verbs <-> lora_radio's typed API, and drains its
 * event queue into [EVT] kind=lora lines on a small dedicated task.
 *
 * radio_arbiter.c doesn't model a LoRa lane (DESIGN §6.2 defers the full
 * two-lane interlock to P6), so ocp_server.c's STOP handler calls
 * lora_cmd_stop() directly, idempotently, rather than going through the PHY
 * arbiter — and only when the requested lane covers LoRa (D-16).
 *
 * SPDX-License-Identifier: MIT
 */

#include "lora_recon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lora_radio.h"
#include "ocp.h"
#include "ocp_frame.h"

static const char *TAG = "lora_recon";

static bool s_ready;
static bool s_configured;
static lora_rx_params_t s_params;

esp_err_t lora_recon_init(void)
{
    esp_err_t err = lora_radio_init();
    s_ready = (err == ESP_OK);
    if (err != ESP_OK) ESP_LOGE(TAG, "lora_radio_init: %s", esp_err_to_name(err));
    return err;
}

bool lora_recon_ready(void) { return s_ready; }

/* kHz -> SX1262 bandwidth code (datasheet Table 13-48; not sequential with
 * kHz, so this is a real lookup, not arithmetic). Accepts the conventional
 * integer kHz values LoRa tooling uses (7 stands in for 7.8, etc). */
static bool parse_bw(const char *s, lora_bw_t *out)
{
    long khz = strtol(s, NULL, 10);
    switch (khz) {
        case 7:   *out = LORA_BW_7_8;   return true;
        case 10:  *out = LORA_BW_10_4;  return true;
        case 15:  *out = LORA_BW_15_6;  return true;
        case 20:  *out = LORA_BW_20_8;  return true;
        case 31:  *out = LORA_BW_31_25; return true;
        case 41:  *out = LORA_BW_41_7;  return true;
        case 62:  *out = LORA_BW_62_5;  return true;
        case 125: *out = LORA_BW_125;   return true;
        case 250: *out = LORA_BW_250;   return true;
        case 500: *out = LORA_BW_500;   return true;
        default:  return false;
    }
}

static int bw_to_khz(lora_bw_t bw)
{
    switch (bw) {
        case LORA_BW_7_8:   return 7;
        case LORA_BW_10_4:  return 10;
        case LORA_BW_15_6:  return 15;
        case LORA_BW_20_8:  return 20;
        case LORA_BW_31_25: return 31;
        case LORA_BW_41_7:  return 41;
        case LORA_BW_62_5:  return 62;
        case LORA_BW_125:   return 125;
        case LORA_BW_250:   return 250;
        case LORA_BW_500:   return 500;
    }
    return 0;
}

/* lora_config <freq_hz> <sf> <bw_khz> <cr>. No default frequency (D-9: no
 * region plan since receive-only) — the caller always states one. */
void lora_cmd_config(int argc, char **argv)
{
    (void)argc;   /* dispatch() already enforced exactly 4 args */
    uint32_t freq = (uint32_t)strtoul(argv[1], NULL, 10);
    long sf = strtol(argv[2], NULL, 10);
    long cr = strtol(argv[4], NULL, 10);
    lora_bw_t bw;

    if (freq < 150000000UL || freq > 960000000UL) {
        ocp_emit_error(OCP_ERR_BADARG, "freq out of SX1262 range");
        return;
    }
    if (sf < 5 || sf > 12) {
        ocp_emit_error(OCP_ERR_BADARG, "sf must be 5..12");
        return;
    }
    if (!parse_bw(argv[3], &bw)) {
        ocp_emit_error(OCP_ERR_BADARG, "bw must be one of 7/10/15/20/31/41/62/125/250/500");
        return;
    }
    if (cr < 1 || cr > 4) {
        ocp_emit_error(OCP_ERR_BADARG, "cr must be 1..4 (4/5..4/8)");
        return;
    }

    s_params.freq_hz = freq;
    s_params.sf = (uint8_t)sf;
    s_params.bw = bw;
    s_params.cr = (uint8_t)cr;
    s_configured = true;

    ocp_emit_compact(OCP_MARK_CFG, "%s=%lu %s=%ld %s=%d %s=%ld",
                     OCP_K_FREQ, (unsigned long)freq, OCP_K_SF, sf,
                     OCP_K_BW, bw_to_khz(bw), OCP_K_CR, cr);
}

static void emit_packet(const lora_packet_t *p)
{
    char hex[LORA_MAX_PAYLOAD * 2 + 1];
    for (int i = 0; i < p->len; i++) snprintf(&hex[i * 2], 3, "%02x", p->payload[i]);

    ocp_emit_event(OCP_EVT_KIND_LORA, "%s=%d %s=%.1f %s=%u %s=%s",
                   OCP_K_RSSI, p->rssi_dbm, OCP_K_SNR, (double)p->snr_db,
                   OCP_K_LEN, (unsigned)p->len, OCP_K_HEX, hex);
}

/* Drains lora_radio's event queue until the radio stops, then exits itself
 * — no external handle needed to tear it down (mirrors the drain-until-done
 * shape lora_radio_rx_stop() already assumes). */
static void drain_task(void *arg)
{
    (void)arg;
    lora_event_t evt;
    while (lora_radio_is_running()) {
        if (!lora_radio_next_event(&evt, 200)) continue;
        switch (evt.kind) {
            case LORA_EVT_PACKET:     emit_packet(&evt.packet); break;
            case LORA_EVT_TIMEOUT:    break;   /* continuous RX: expected, not an event worth a line */
            case LORA_EVT_CRC_ERR:    break;   /* counted implicitly by absence of packets; no counter yet */
            case LORA_EVT_HEADER_ERR: break;
        }
    }
    vTaskDelete(NULL);
}

void lora_cmd_listen(void)
{
    if (!s_configured) {
        ocp_emit_error(OCP_ERR_BADARG, "lora_config required first");
        return;
    }
    if (lora_radio_is_running()) {
        ocp_emit_error(OCP_ERR_BUSY, "already listening");
        return;
    }

    esp_err_t err = lora_radio_rx_start(&s_params);
    if (err != ESP_OK) {
        ocp_emit_error(OCP_ERR_HWFAULT, esp_err_to_name(err));
        return;
    }

    if (xTaskCreate(drain_task, "lora_drain", 4096, NULL, 5, NULL) != pdPASS) {
        lora_radio_rx_stop();
        ocp_emit_error(OCP_ERR_INTERNAL, "could not start drain task");
        return;
    }

    ocp_emit_compact(OCP_MARK_LORA, "%s=%lu %s=%d %s=%d %s=%ld",
                     OCP_K_FREQ, (unsigned long)s_params.freq_hz, OCP_K_SF, s_params.sf,
                     OCP_K_BW, bw_to_khz(s_params.bw), OCP_K_CR, (long)s_params.cr);
}

void lora_cmd_status(void)
{
    bool running = lora_radio_is_running();
    if (!s_configured) {
        ocp_emit_compact(OCP_MARK_LORA, "%s=%d configured=0", OCP_K_RUNNING, running ? 1 : 0);
        return;
    }
    ocp_emit_compact(OCP_MARK_LORA, "%s=%d %s=%lu %s=%d %s=%d %s=%ld",
                     OCP_K_RUNNING, running ? 1 : 0,
                     OCP_K_FREQ, (unsigned long)s_params.freq_hz, OCP_K_SF, s_params.sf,
                     OCP_K_BW, bw_to_khz(s_params.bw), OCP_K_CR, (long)s_params.cr);
}

bool lora_cmd_stop(void)
{
    bool was_running = lora_radio_is_running();
    lora_radio_rx_stop();   /* safe even if not running; drain_task notices and self-exits */
    return was_running;
}
