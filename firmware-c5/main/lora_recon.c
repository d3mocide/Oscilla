/*
 * lora_recon.c — see lora_recon.h.
 *
 * Owns nothing electrical: all SPI/GPIO work lives in lora_radio.c. This
 * file only translates OCP verbs <-> lora_radio's typed API, and drains its
 * event queue into [EVT] kind=lora lines on a small dedicated task.
 *
 * radio_arbiter.c doesn't model a LoRa lane (DESIGN §6.2 defers the full
 * PHY/LoRa interlock to P6), so ocp_server.c's STOP handler calls
 * lora_cmd_stop() directly, idempotently, rather than going through the PHY
 * arbiter — and only when the requested lane covers LoRa (D-16).
 *
 * SPDX-License-Identifier: MIT
 */

#include "lora_recon.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lora_radio.h"
#include "ocp.h"
#include "ocp_frame.h"
#include "ocp_parse.h"

static const char *TAG = "lora_recon";

/* Bounded wait for a previous drain_task to actually exit before starting a
 * new one — same idiom as zig_teardown()'s wait for its own task
 * (zig_recon.c). Without this, a rapid stop-then-listen can leave the old task
 * still blocked in lora_radio_next_event() when rx_start() flips s_running
 * back to true, and it never notices it should have exited. */
#define DRAIN_WAIT_MS 250

static bool s_ready;
static bool s_configured;
static lora_rx_params_t s_params;
static volatile bool s_drain_idle = true;
static uint32_t s_ocp_lora_drop_start;

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
    uint32_t khz = 0;
    if (!ocp_parse_u32(s, &khz)) return false;
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

/* RadioLib's own default (see lora_radio.c's lora_set_sync_word citation) —
 * an omitted 5th arg keeps existing 4-arg callers behaving exactly as
 * before, since this is also MeshCore's real sync word. */
#define SYNC_WORD_DEFAULT 0x12

/* lora_config <freq_hz> <sf> <bw_khz> <cr> [sync_word]. No default frequency
 * (D-9: no region plan since receive-only) — the caller always states one.
 * sync_word is optional and decimal (0..255) on the wire; omitted means
 * SYNC_WORD_DEFAULT. */
void lora_cmd_config(int argc, char **argv)
{
    uint32_t freq = 0, sf = 0, cr = 0, sync = SYNC_WORD_DEFAULT;
    lora_bw_t bw;

    if (!ocp_parse_u32(argv[1], &freq) || !ocp_parse_u32(argv[2], &sf) ||
        !ocp_parse_u32(argv[4], &cr)) {
        ocp_emit_error(OCP_ERR_BADARG, "freq/sf/bw/cr must be whole unsigned integers");
        return;
    }
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
    if (argc == 6) {   /* argc counts argv[0]=verb too; 6 means a 5th arg (sync_word) is present */
        if (!ocp_parse_u32(argv[5], &sync) || sync > 255) {
            ocp_emit_error(OCP_ERR_BADARG, "sync_word must be 0..255");
            return;
        }
    }

    s_params.freq_hz = freq;
    s_params.sf = (uint8_t)sf;
    s_params.bw = bw;
    s_params.cr = (uint8_t)cr;
    s_params.sync_word = (uint8_t)sync;
    s_configured = true;

    ocp_emit_compact(OCP_MARK_CFG, "%s=%lu %s=%lu %s=%d %s=%lu %s=%lu",
                     OCP_K_FREQ, (unsigned long)freq, OCP_K_SF, (unsigned long)sf,
                     OCP_K_BW, bw_to_khz(bw), OCP_K_CR, (unsigned long)cr,
                     OCP_K_SYNC, (unsigned long)sync);
}

static void emit_packet(const lora_packet_t *p)
{
    ocp_event_record_t event = { .kind = OCP_EVENT_RECORD_LORA };
    event.data.lora.len = p->len;
    memcpy(event.data.lora.payload, p->payload, p->len);
    event.data.lora.rssi_dbm = p->rssi_dbm;
    event.data.lora.snr_db = p->snr_db;
    (void)ocp_event_submit(&event);
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
    s_drain_idle = true;
    vTaskDelete(NULL);
}

static void lora_teardown(void)
{
    lora_radio_rx_stop();
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
    for (uint32_t waited = 0; !s_drain_idle && waited < DRAIN_WAIT_MS; waited += 5) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    esp_err_t err = lora_radio_rx_start(&s_params);
    if (err != ESP_OK) {
        ocp_emit_error(OCP_ERR_HWFAULT, esp_err_to_name(err));
        return;
    }
    s_ocp_lora_drop_start = ocp_event_drop_count(OCP_EVENT_RECORD_LORA);

    s_drain_idle = false;
    if (xTaskCreate(drain_task, "lora_drain", 4096, NULL, 5, NULL) != pdPASS) {
        s_drain_idle = true;
        lora_teardown();
        ocp_emit_error(OCP_ERR_INTERNAL, "could not start drain task");
        return;
    }

    ocp_emit_compact(OCP_MARK_LORA, "%s=%lu %s=%d %s=%d %s=%ld %s=%d",
                     OCP_K_FREQ, (unsigned long)s_params.freq_hz, OCP_K_SF, s_params.sf,
                     OCP_K_BW, bw_to_khz(s_params.bw), OCP_K_CR, (long)s_params.cr,
                     OCP_K_SYNC, s_params.sync_word);
}

void lora_cmd_status(void)
{
    bool running = lora_radio_is_running();
    lora_radio_stats_t stats = { 0 };
    lora_radio_get_stats(&stats);
    uint32_t ocp_drop = ocp_event_drop_count(OCP_EVENT_RECORD_LORA) - s_ocp_lora_drop_start;
    if (!s_configured) {
        ocp_emit_compact(OCP_MARK_LORA, "%s=%d configured=0 %s=0 %s=0 %s=0 %s=0 %s=0 %s=%lu %s=0",
                         OCP_K_RUNNING, running ? 1 : 0,
                         OCP_K_RX, OCP_K_CRC_ERR, OCP_K_HEADER_ERR, OCP_K_IRQ_DROP, OCP_K_RADIO_DROP,
                         OCP_K_OCP_DROP, (unsigned long)ocp_drop, OCP_K_HW_FAULT);
        return;
    }
    ocp_emit_compact(OCP_MARK_LORA, "%s=%d %s=%lu %s=%d %s=%d %s=%ld %s=%d %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu",
                     OCP_K_RUNNING, running ? 1 : 0,
                     OCP_K_FREQ, (unsigned long)s_params.freq_hz, OCP_K_SF, s_params.sf,
                     OCP_K_BW, bw_to_khz(s_params.bw), OCP_K_CR, (long)s_params.cr,
                     OCP_K_SYNC, s_params.sync_word,
                     OCP_K_RX, (unsigned long)stats.rx,
                     OCP_K_CRC_ERR, (unsigned long)stats.crc_err,
                     OCP_K_HEADER_ERR, (unsigned long)stats.header_err,
                     OCP_K_IRQ_DROP, (unsigned long)stats.irq_drop,
                     OCP_K_RADIO_DROP, (unsigned long)stats.event_drop,
                     OCP_K_OCP_DROP, (unsigned long)ocp_drop,
                     OCP_K_HW_FAULT, (unsigned long)stats.hw_fault);
}

bool lora_cmd_stop(void)
{
    if (!lora_radio_is_running()) return false;
    lora_teardown();
    return true;
}
