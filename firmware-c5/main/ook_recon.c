/*
 * ook_recon.c — see ook_recon.h.
 *
 * Data path: cc1101_capture (8 ms drain task, priority 6) -> this engine's
 * task (priority 4, below the OCP event sender's consumers' needs but not
 * spinning: it blocks on the capture buffer) -> ook_pulse -> ook_decode ->
 * ook_events -> ocp_event_submit. The engine never formats text.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ook_recon.h"

#include <string.h>

#include "cc1101_bus.h"
#include "cc1101_capture.h"
#include "cc1101_config.h"
#include "cc1101_port.h"
#include "cc1101_profile.h"
#include "cc1101_rx.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ocp.h"
#include "ocp_frame.h"
#include "ocp_parse.h"
#include "ook_decode.h"
#include "ook_events.h"
#include "ook_pulse.h"

#define TAG "ook"

#define OOK_BAUD 25000u               /* WORKLOG 2026-11-23: 10/30-bit Princeton runs, LaCrosse resolved */
#define OOK_GAP_US 6000u
#define OOK_GLITCH_US 100u
#define OOK_DEFAULT_KHZ 433920u
#define OOK_TASK_PRIO 4               /* below capture (6) and OCP dispatch (10) */
#define OOK_TASK_STACK 2560
#define DRIFT_CHECK_MS 1000u
#define STOP_WAIT_MS 500u

static cc1101_bus_t s_bus;
static cc1101_profile_t s_profile;
static bool s_ready;
static volatile bool s_running;
static volatile bool s_task_alive;
static uint32_t s_freq_khz = OOK_DEFAULT_KHZ;
static uint32_t s_baud;

/* Session state: owned by the engine task while it runs. */
static ook_pulse_t s_pulse;
static ook_events_t s_events;
static int16_t s_rssi_peak;
static uint32_t s_seen_overflows;
static uint32_t s_ocp_drop_start;

/* Counters (written by the engine task, read by `ook_status`). */
static volatile uint32_t s_trains, s_packets, s_unknown, s_reinit, s_hw_fault;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

bool ook_recon_ready(void) { return s_ready; }
bool ook_recon_is_running(void) { return s_running; }

/* Park in SLEEP between sessions (SPWD; IDLE draws ~1.7 mA). Registers
 * 0x29-0x2E (TEST2..0 among them) lose their values in SLEEP (SWRS061I
 * §29.2), so every listen re-applies and verifies the full profile. */
static void park(void)
{
    (void)cc1101_rx_stop(&s_bus);
    (void)cc1101_strobe(&s_bus, CC1101_SPWD, NULL);
}

static cc1101_err_t make_profile(uint32_t freq_khz, cc1101_profile_t *out, uint32_t *baud)
{
    cc1101_profile_t p = cc1101_profile_ook_433_async;
    if (!cc1101_profile_set_freq(&p, freq_khz * 1000u)) return CC1101_ERR_ARG;
    const uint32_t got = cc1101_profile_make_fifo(&p, OOK_BAUD);
    if (!got) return CC1101_ERR_ARG;
    *out = p;
    *baud = got;
    return CC1101_OK;
}

esp_err_t ook_recon_init(void)
{
    esp_err_t err = cc1101_port_init(&s_bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "port init: %s", esp_err_to_name(err));
        return err;
    }
    uint8_t part = 0, ver = 0;
    cc1101_err_t c = cc1101_probe(&s_bus, &part, &ver);
    if (c != CC1101_OK) {
        ESP_LOGI(TAG, "no CC1101 (%s): ook lane absent", cc1101_err_name(c));
        return ESP_OK;
    }
    bool edge = false;
    c = cc1101_reset(&s_bus, &edge);
    if (c == CC1101_OK) c = make_profile(s_freq_khz, &s_profile, &s_baud);
    if (c == CC1101_OK) c = cc1101_apply_profile(&s_bus, &s_profile, NULL);
    if (c != CC1101_OK) {
        ESP_LOGW(TAG, "CC1101 0x%02x/0x%02x bring-up failed: %s", part, ver, cc1101_err_name(c));
        return ESP_OK;
    }
    err = cc1101_capture_init(&s_bus);
    if (err != ESP_OK) return err;
    park();
    s_ready = true;
    ESP_LOGI(TAG, "CC1101 partnum 0x%02x version 0x%02x ready", part, ver);
    return ESP_OK;
}

static void submit_packet(const ook_packet_t *p, int16_t rssi)
{
    ocp_event_record_t ev = { .kind = OCP_EVENT_RECORD_OOK };
    ev.data.ook.pk = *p;
    ev.data.ook.rssi_dbm = rssi;
    (void)ocp_event_submit(&ev);
}

static void on_train(const ook_train_t *t, void *ctx)
{
    (void)ctx;
    s_trains++;
    const int16_t rssi = s_rssi_peak;
    s_rssi_peak = INT16_MIN;
    ook_packet_t pk[4];
    const size_t n = ook_decode(t, pk, 4);
    for (size_t i = 0; i < n; i++) {
        if (!ook_events_packet(&s_events, &pk[i], t->end_ms)) continue;
        s_packets++;
        submit_packet(&pk[i], rssi);
    }
    if (n) return;
    s_unknown++;
    uint32_t count = 0;
    if (!ook_events_unknown(&s_events, t->end_ms, &count)) return;
    ook_train_summary_t sm;
    ook_train_summarise(t, &sm);
    ocp_event_record_t ev = { .kind = OCP_EVENT_RECORD_OOK_UNKNOWN };
    ev.data.ook_unknown = (ocp_event_ook_unknown_t){ count, sm.pulses, sm.min_us, sm.max_us, sm.dur_ms, rssi };
    (void)ocp_event_submit(&ev);
}

/* Profile drift or a silent reset (the September failure mode): re-apply
 * and restart RX. RXFIFO_OVERFLOW is the capture task's to handle. */
static void check_drift(void)
{
    cc1101_capture_pause();
    int16_t at = -1;
    uint8_t marc = 0;
    cc1101_err_t v = cc1101_verify_profile(&s_bus, &s_profile, &at);
    const cc1101_err_t m = cc1101_read_status(&s_bus, CC1101_MARCSTATE, &marc);
    marc &= 0x1F;
    const bool ok = v == CC1101_OK && m == CC1101_OK &&
                    (marc == CC1101_MARC_RX || marc == CC1101_MARC_RXFIFO_OVERFLOW);
    if (!ok) {
        s_reinit++;
        v = cc1101_apply_profile(&s_bus, &s_profile, NULL);
        if (v == CC1101_OK) v = cc1101_rx_start(&s_bus, NULL);
        if (v != CC1101_OK) s_hw_fault++;
        ook_pulse_reset(&s_pulse);
        ESP_LOGW(TAG, "drift (verify first_mismatch=%d marc=0x%02x): re-applied, %s", at, marc,
                 cc1101_err_name(v));
    }
    cc1101_capture_resume();
}

static void engine_task(void *arg)
{
    (void)arg;
    uint32_t last_check = now_ms();
    cc1101_chunk_t c;
    while (s_running) {
        if (cc1101_capture_read(&c, 100)) {
            cc1101_capture_stats_t cs;
            cc1101_capture_get_stats(&cs);
            if (cs.overflows != s_seen_overflows) {   /* samples were lost: no train spans the hole */
                s_seen_overflows = cs.overflows;
                ook_pulse_reset(&s_pulse);
            }
            if (c.rssi_dbm != INT16_MIN && (s_rssi_peak == INT16_MIN || c.rssi_dbm > s_rssi_peak))
                s_rssi_peak = c.rssi_dbm;
            ook_pulse_feed(&s_pulse, c.data, c.n, c.t_ms, on_train, NULL);
        }
        if (s_running && now_ms() - last_check >= DRIFT_CHECK_MS) {
            last_check = now_ms();
            check_drift();
        }
    }
    s_task_alive = false;
    vTaskDelete(NULL);
}

void ook_cmd_listen(int argc, char **argv)
{
    uint32_t khz = s_freq_khz;
    if (argc > 1 && !ocp_parse_u32(argv[1], &khz)) {
        ocp_emit_error(OCP_ERR_BADARG, "freq_khz must be a whole unsigned integer");
        return;
    }
    if (s_running || s_task_alive) {
        ocp_emit_error(OCP_ERR_BUSY, "already listening; stop ook first");
        return;
    }
    cc1101_profile_t p;
    uint32_t baud = 0;
    if (khz > UINT32_MAX / 1000u || make_profile(khz, &p, &baud) != CC1101_OK) {
        ocp_emit_error(OCP_ERR_BADARG, "freq_khz outside 300000-348000, 387000-464000, 779000-928000");
        return;
    }
    bool cal = false;
    cc1101_err_t c = cc1101_apply_profile(&s_bus, &p, NULL);   /* wakes the chip from SLEEP */
    if (c == CC1101_OK) c = cc1101_rx_start(&s_bus, &cal);
    if (c != CC1101_OK) {
        park();
        ocp_emit_error(OCP_ERR_HWFAULT, cc1101_err_name(c));
        return;
    }
    s_profile = p;
    s_freq_khz = khz;
    s_baud = baud;
    ook_pulse_init(&s_pulse, baud, OOK_GAP_US, OOK_GLITCH_US);
    ook_events_init(&s_events, OOK_EVENTS_WINDOW_MS, OOK_EVENTS_UNKNOWN_MS);
    s_rssi_peak = INT16_MIN;
    s_seen_overflows = 0;
    s_trains = s_packets = s_unknown = s_reinit = s_hw_fault = 0;
    s_ocp_drop_start = ocp_event_drop_count(OCP_EVENT_RECORD_OOK) + ocp_event_drop_count(OCP_EVENT_RECORD_OOK_UNKNOWN);

    if (cc1101_capture_start(CC1101_CAPTURE_PERIOD_MS_DEFAULT) != ESP_OK) {
        park();
        ocp_emit_error(OCP_ERR_HWFAULT, "capture start");
        return;
    }
    s_running = true;
    s_task_alive = true;
    if (xTaskCreate(engine_task, "ook", OOK_TASK_STACK, NULL, OOK_TASK_PRIO, NULL) != pdPASS) {
        s_running = false;
        s_task_alive = false;
        cc1101_capture_stop();
        park();
        ocp_emit_error(OCP_ERR_INTERNAL, "could not start ook task");
        return;
    }
    ocp_emit_compact(OCP_MARK_OOK, "%s=1 %s=%lu %s=%lu", OCP_K_RUNNING, OCP_K_FREQ_KHZ,
                     (unsigned long)s_freq_khz, OCP_K_BAUD, (unsigned long)s_baud);
}

void ook_cmd_status(void)
{
    cc1101_capture_stats_t cs = { 0 };
    if (s_running) cc1101_capture_get_stats(&cs);
    const uint32_t drop = ocp_event_drop_count(OCP_EVENT_RECORD_OOK) +
                          ocp_event_drop_count(OCP_EVENT_RECORD_OOK_UNKNOWN) - s_ocp_drop_start;
    ocp_emit_compact(OCP_MARK_OOK,
                     "%s=%d %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu %s=%lu",
                     OCP_K_RUNNING, s_running ? 1 : 0, OCP_K_FREQ_KHZ, (unsigned long)s_freq_khz,
                     OCP_K_BAUD, (unsigned long)s_baud, OCP_K_TRAINS, (unsigned long)s_trains,
                     OCP_K_PACKETS, (unsigned long)s_packets, OCP_K_UNKNOWN, (unsigned long)s_unknown,
                     OCP_K_OVERFLOWS, (unsigned long)cs.overflows, OCP_K_DROPPED, (unsigned long)cs.dropped,
                     OCP_K_ERRORS, (unsigned long)cs.errors, OCP_K_REINIT, (unsigned long)s_reinit,
                     OCP_K_HW_FAULT, (unsigned long)s_hw_fault, OCP_K_OCP_DROP, (unsigned long)drop);
}

bool ook_cmd_stop(void)
{
    if (!s_ready) return false;
    const bool was = s_running;
    s_running = false;
    for (uint32_t waited = 0; s_task_alive && waited < STOP_WAIT_MS; waited += 5) vTaskDelay(pdMS_TO_TICKS(5));
    cc1101_capture_stop();
    if (was) park();
    return was;
}
