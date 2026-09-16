/*
 * ble_recon.c — see ble_recon.h.
 *
 * Completion is NOT the same shape as wifi_recon.c's scan_teardown(), on
 * purpose — confirmed the hard way on hardware 2026-09-16 (see WORKLOG).
 * esp_wifi_scan_stop() always completes asynchronously through the same
 * event wifi_recon.c's teardown waits on, whether the scan ended naturally
 * or was cancelled — so waiting for that event is correct there.
 * ble_gap_disc_cancel() does not make that promise: per its own doc
 * comment (host/ble_gap.h), "a success return code indicates that
 * scanning has been fully aborted" — synchronously, by the time the call
 * returns. It does not emit BLE_GAP_EVENT_DISC_COMPLETE. Waiting for that
 * event after cancelling an unbounded (BLE_HS_FOREVER) scan_airtag session
 * left the radio arbiter stuck forever, since nothing was ever going to
 * fire it. finish_scan() is called from both places instead — the natural
 * completion path (gap_event's own DISC_COMPLETE, which *does* fire for a
 * bounded scan_bt session that runs to the end of its own dwell) and the
 * forced path (ble_teardown(), immediately after a synchronous cancel) —
 * and is idempotent (checks s_mode first) so whichever gets there first
 * for a given session wins; the other is a no-op, not a double release.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ble_recon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "ble_adv_parse.h"
#include "ble_device_table.h"
#include "ocp.h"
#include "ocp_frame.h"
#include "ocp_text.h"
#include "radio_arbiter.h"

static const char *TAG = "ble";

typedef enum { BLE_MODE_NONE, BLE_MODE_SCAN_BT, BLE_MODE_SCAN_AIRTAG } ble_mode_t;

static SemaphoreHandle_t s_lock;
static bool s_ready;

static ble_mode_t s_mode;
static ble_device_table_t s_table;
static int64_t s_started_us;
static uint32_t s_elapsed_ms;
static uint32_t s_dwell_ms;
static uint32_t s_airtag_n;

/* Caller holds s_lock. */
static void emit_ble_frame(bool aborted)
{
    ocp_emit_begin(OCP_MARK_BLE, "%s=%u %s=%u %s=%u %s=%u%s",
                   OCP_K_COUNT, s_table.count, OCP_K_TOTAL, s_table.count,
                   OCP_K_DWELL_MS, s_dwell_ms, OCP_K_ELAPSED_MS, s_elapsed_ms,
                   aborted ? " " OCP_K_ABORTED "=1" : "");

    for (unsigned i = 0; i < s_table.count; i++) {
        const ble_device_t *d = &s_table.devices[i];
        char name[4 * 32 + 3];
        ocp_escape_field(d->name, d->name_len, name, sizeof name);
        char mfr[8];
        if (d->has_mfr) snprintf(mfr, sizeof mfr, "\"%04x\"", d->mfr_company_id);
        else snprintf(mfr, sizeof mfr, "\"\"");
        const uint8_t *a = d->addr;
        ocp_emit_row(OCP_MARK_BLE,
                     "\"%02x:%02x:%02x:%02x:%02x:%02x\",%s,%s,\"%s\",\"%d\",\"%lu\"",
                     a[0], a[1], a[2], a[3], a[4], a[5], name, mfr,
                     d->is_tracker ? OCP_EVT_KIND_AIRTAG : "", d->rssi,
                     (unsigned long)d->n);
    }
    ocp_emit_end(OCP_MARK_BLE);
}

/* Caller holds s_lock, leaves it released. Idempotent: a no-op if the
 * session already finished by the other path (see file header) — safe to
 * call from both gap_event()'s natural DISC_COMPLETE and ble_teardown()'s
 * forced cancel without risking a double release or a duplicate frame. */
static void finish_scan(bool aborted)
{
    if (s_mode == BLE_MODE_NONE) { xSemaphoreGive(s_lock); return; }
    bool was_bt = (s_mode == BLE_MODE_SCAN_BT);
    s_elapsed_ms = (uint32_t)((esp_timer_get_time() - s_started_us) / 1000);
    s_mode = BLE_MODE_NONE;
    arbiter_release(PHY_OWNER_BLE);
    if (was_bt) emit_ble_frame(aborted);
    xSemaphoreGive(s_lock);
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (s_mode == BLE_MODE_NONE) { xSemaphoreGive(s_lock); return 0; }

        ble_adv_info_t info;
        ble_adv_parse(event->disc.data, event->disc.length_data, &info);
        ble_device_table_upsert(&s_table, event->disc.addr.val, &info, event->disc.rssi);

        bool report_tracker = (s_mode == BLE_MODE_SCAN_AIRTAG) && info.is_tracker;
        uint32_t n = report_tracker ? ++s_airtag_n : 0;
        uint8_t addr[6];
        memcpy(addr, event->disc.addr.val, 6);
        int8_t rssi = event->disc.rssi;
        xSemaphoreGive(s_lock);

        if (report_tracker) {
            ocp_emit_event(OCP_EVT_KIND_AIRTAG, "%s=%02x:%02x:%02x:%02x:%02x:%02x %s=%d %s=%lu",
                           OCP_K_MAC, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
                           OCP_K_RSSI, rssi, OCP_K_COUNT, (unsigned long)n);
        }
        return 0;
    }

    case BLE_GAP_EVENT_DISC_COMPLETE: {
        /* Only ever reached naturally (scan_bt's dwell expiring) — a
         * scan_airtag session runs with BLE_HS_FOREVER, so its only exit is
         * ble_teardown()'s forced path. aborted=false: this is what
         * "finished" looks like, not what a `stop` looks like. */
        xSemaphoreTake(s_lock, portMAX_DELAY);
        finish_scan(false);
        return 0;
    }

    default:
        return 0;
    }
}

static int start_disc(int32_t duration_ms)
{
    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) return rc;

    struct ble_gap_disc_params params = {0};
    params.passive = 1;             /* receive-only: never a scan request (§11.1) */
    params.filter_duplicates = 0;   /* every sighting should bump the table's rssi/n, not just the first */

    return ble_gap_disc(own_addr_type, duration_ms, &params, gap_event, NULL);
}

/* Dispatch task, via arbiter_stop_all(). No async wait here (see file
 * header): ble_gap_disc_cancel() is done by the time it returns. */
static void ble_teardown(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_mode == BLE_MODE_NONE) { xSemaphoreGive(s_lock); return; }
    xSemaphoreGive(s_lock);

    ble_gap_disc_cancel();   /* BLE_HS_EALREADY just means it already finished naturally */

    xSemaphoreTake(s_lock, portMAX_DELAY);
    finish_scan(true);
}

void ble_cmd_scan_bt(int argc, char **argv)
{
    uint32_t dwell_ms = BLE_SCAN_DWELL_MS;
    if (argc > 1) {
        char *end;
        long v = strtol(argv[1], &end, 10);
        if (*end != '\0' || v <= 0) {
            ocp_emit_error(OCP_ERR_BADARG, "dwell_ms must be a positive integer");
            return;
        }
        dwell_ms = (uint32_t)v;
    }

    if (arbiter_acquire(PHY_OWNER_BLE, ble_teardown) != ESP_OK) {
        char msg[48];
        snprintf(msg, sizeof msg, "radio in use by %s", arbiter_owner_name(arbiter_owner()));
        ocp_emit_error(OCP_ERR_BUSY, msg);
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    ble_device_table_reset(&s_table);
    s_mode = BLE_MODE_SCAN_BT;
    s_dwell_ms = dwell_ms;
    s_started_us = esp_timer_get_time();
    xSemaphoreGive(s_lock);

    if (start_disc((int32_t)dwell_ms) != 0) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_mode = BLE_MODE_NONE;
        xSemaphoreGive(s_lock);
        arbiter_release(PHY_OWNER_BLE);
        ocp_emit_error(OCP_ERR_HWFAULT, "ble_gap_disc failed");
    }
}

void ble_cmd_scan_airtag(void)
{
    if (arbiter_acquire(PHY_OWNER_BLE, ble_teardown) != ESP_OK) {
        char msg[48];
        snprintf(msg, sizeof msg, "radio in use by %s", arbiter_owner_name(arbiter_owner()));
        ocp_emit_error(OCP_ERR_BUSY, msg);
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_mode = BLE_MODE_SCAN_AIRTAG;
    s_airtag_n = 0;
    xSemaphoreGive(s_lock);

    if (start_disc(BLE_HS_FOREVER) != 0) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_mode = BLE_MODE_NONE;
        xSemaphoreGive(s_lock);
        arbiter_release(PHY_OWNER_BLE);
        ocp_emit_error(OCP_ERR_HWFAULT, "ble_gap_disc failed");
        return;
    }
    ocp_emit_compact(OCP_MARK_CFG, "%s", "");
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "NimBLE host reset; reason=%d", reason);
}

static void on_sync(void)
{
    ble_hs_util_ensure_addr(0);
    s_ready = true;
}

static void host_task(void *param)
{
    (void)param;
    nimble_port_run();   /* returns only after nimble_port_stop() */
    nimble_port_freertos_deinit();
}

esp_err_t ble_recon_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) return err;

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;

    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

bool ble_recon_ready(void) { return s_ready; }
