/*
 * wifi_recon.c — see wifi_recon.h.
 *
 * Locking: s_lock (engine state) is always taken before the frame lock inside
 * ocp_frame, never the reverse.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_recon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ocp.h"
#include "ocp_parse.h"
#include "ocp_frame.h"
#include "ocp_text.h"
#include "radio_arbiter.h"

static const char *TAG = "wifi";

static SemaphoreHandle_t s_lock;
static SemaphoreHandle_t s_done;          /* given when a scan's frame is out */
static bool s_ready;
static bool s_scanning;
static bool s_aborting;
static int64_t s_started_us;
static uint32_t s_elapsed_ms;

static wifi_ap_record_t *s_results;
static uint16_t s_total;

static wifi_scan_config_t wifi_passive_scan_config(void)
{
    return (wifi_scan_config_t){
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_PASSIVE,   /* receive-only: no probe requests (D-8) */
        .scan_time.passive = WIFI_SCAN_DWELL_MS,
    };
}

static const char *auth_label(wifi_auth_mode_t a)
{
    switch (a) {
        case WIFI_AUTH_OPEN:                 return OCP_AUTH_OPEN;
        case WIFI_AUTH_WEP:                  return OCP_AUTH_WEP;
        case WIFI_AUTH_WPA_PSK:              return OCP_AUTH_WPA;
        case WIFI_AUTH_WPA2_PSK:             return OCP_AUTH_WPA2;
        case WIFI_AUTH_WPA_WPA2_PSK:         return OCP_AUTH_WPA_WPA2;
        case WIFI_AUTH_WPA2_ENTERPRISE:      return OCP_AUTH_WPA2_EAP;
        case WIFI_AUTH_WPA3_PSK:
        case WIFI_AUTH_WPA3_EXT_PSK:
        case WIFI_AUTH_WPA3_EXT_PSK_MIXED_MODE: return OCP_AUTH_WPA3;
        case WIFI_AUTH_WPA2_WPA3_PSK:        return OCP_AUTH_WPA2_WPA3;
        case WIFI_AUTH_WAPI_PSK:             return OCP_AUTH_WAPI;
        case WIFI_AUTH_OWE:                  return OCP_AUTH_OWE;
        case WIFI_AUTH_WPA3_ENT_192:         return OCP_AUTH_WPA3_EAP192;
        case WIFI_AUTH_DPP:                  return OCP_AUTH_DPP;
        case WIFI_AUTH_WPA3_ENTERPRISE:      return OCP_AUTH_WPA3_EAP;
        case WIFI_AUTH_WPA2_WPA3_ENTERPRISE: return OCP_AUTH_WPA2_WPA3_EAP;
        case WIFI_AUTH_WPA_ENTERPRISE:       return OCP_AUTH_WPA_EAP;
        default:                             return OCP_AUTH_UNKNOWN;
    }
}

static int by_rssi_desc(const void *a, const void *b)
{
    const wifi_ap_record_t *x = a, *y = b;
    if (x->rssi != y->rssi) return y->rssi - x->rssi;
    return memcmp(x->bssid, y->bssid, 6);   /* deterministic order on ties */
}

/* Caller holds s_lock. */
static void emit_page(uint16_t first, bool aborted)
{
    uint16_t n = 0;
    if (s_total && first >= 1 && first <= s_total) {
        n = s_total - first + 1;
        if (n > OCP_MAX_FRAME_ROWS) n = OCP_MAX_FRAME_ROWS;
    }

    ocp_emit_begin(OCP_MARK_SCAN, "%s=%u %s=%u %s=%u %s=%u %s=%u%s",
                   OCP_K_COUNT, n, OCP_K_TOTAL, s_total, OCP_K_FIRST, first,
                   OCP_K_DWELL_MS, WIFI_SCAN_DWELL_MS, OCP_K_ELAPSED_MS, s_elapsed_ms,
                   aborted ? " " OCP_K_ABORTED "=1" : "");

    for (uint16_t i = 0; i < n; i++) {
        const wifi_ap_record_t *r = &s_results[first - 1 + i];
        char ssid[4 * 32 + 3];
        ocp_escape_field(r->ssid, strnlen((const char *)r->ssid, 32), ssid, sizeof ssid);
        const uint8_t *b = r->bssid;
        ocp_emit_row(OCP_MARK_SCAN,
                     "\"%u\",%s,\"%02x:%02x:%02x:%02x:%02x:%02x\",\"%u\",\"%s\",\"%d\",\"%s\"",
                     (unsigned)(first + i), ssid, b[0], b[1], b[2], b[3], b[4], b[5],
                     r->primary, auth_label(r->authmode), r->rssi,
                     r->primary <= 14 ? OCP_BAND_LABEL_24 : OCP_BAND_LABEL_5);
    }
    ocp_emit_end(OCP_MARK_SCAN);
}

/* Event task. */
static void on_scan_done(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (!s_scanning) {                     /* teardown already gave up waiting */
        esp_wifi_clear_ap_list();
        xSemaphoreGive(s_lock);
        return;
    }

    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    if (count > WIFI_STORE_MAX) count = WIFI_STORE_MAX;

    wifi_ap_record_t *recs = count
                                ? heap_caps_malloc(count * sizeof *recs,
                                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
                                : NULL;
    if (count && !recs) {
        recs = heap_caps_malloc(count * sizeof *recs,
                                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (count && !recs) {
        ESP_LOGE(TAG, "no memory for %u results", count);
        count = 0;
    }
    if (recs) {
        esp_wifi_scan_get_ap_records(&count, recs);   /* also frees the driver's list */
    } else {
        esp_wifi_clear_ap_list();
    }

    bool aborted = s_aborting;
    if (aborted) {                          /* partial results are not a scan */
        heap_caps_free(recs);
        recs = NULL;
        count = 0;
    } else if (count > 1) {
        qsort(recs, count, sizeof *recs, by_rssi_desc);
    }

    heap_caps_free(s_results);
    s_results = recs;
    s_total = count;
    s_elapsed_ms = (uint32_t)((esp_timer_get_time() - s_started_us) / 1000);
    s_scanning = false;
    s_aborting = false;
    arbiter_release(PHY_OWNER_WIFI);

    emit_page(1, aborted);
    xSemaphoreGive(s_lock);
    xSemaphoreGive(s_done);
}

/* Dispatch task, via arbiter_stop_all(). Returns once the aborted frame is out. */
static void scan_teardown(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool scanning = s_scanning;
    if (scanning) s_aborting = true;
    xSemaphoreGive(s_lock);
    if (!scanning) return;

    xSemaphoreTake(s_done, 0);             /* discard a stale give */
    esp_wifi_scan_stop();
    if (xSemaphoreTake(s_done, pdMS_TO_TICKS(1500)) == pdTRUE) return;

    /* No SCAN_DONE: close the scan ourselves rather than leave the deck hanging. */
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_scanning) {
        heap_caps_free(s_results);
        s_results = NULL;
        s_total = 0;
        s_elapsed_ms = (uint32_t)((esp_timer_get_time() - s_started_us) / 1000);
        s_scanning = false;
        s_aborting = false;
        arbiter_release(PHY_OWNER_WIFI);
        emit_page(1, true);
    }
    xSemaphoreGive(s_lock);
}

/* Each step is named in the log: "NOT_STARTED" alone cost a round trip once. */
#define STEP(call) do { \
    esp_err_t e_ = (call); \
    if (e_ != ESP_OK) { ESP_LOGE(TAG, "%s: %s", #call, esp_err_to_name(e_)); return e_; } \
} while (0)

esp_err_t wifi_recon_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_done = xSemaphoreCreateBinary();
    if (!s_lock || !s_done) return ESP_ERR_NO_MEM;

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    STEP(esp_wifi_init(&cfg));
    STEP(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    STEP(esp_wifi_set_mode(WIFI_MODE_STA));                  /* never associates */
    STEP(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, on_scan_done, NULL));
    STEP(esp_wifi_start());
    STEP(esp_wifi_set_band_mode(WIFI_BAND_MODE_AUTO));        /* only valid once started */
    esp_wifi_set_ps(WIFI_PS_NONE);

    s_ready = true;
    ESP_LOGI(TAG, "ready: passive scan, dwell %d ms, record %u bytes",
             WIFI_SCAN_DWELL_MS, (unsigned)sizeof(wifi_ap_record_t));
    return ESP_OK;
}

bool wifi_recon_ready(void) { return s_ready; }

bool wifi_recon_lookup(unsigned idx, uint8_t bssid[6], uint8_t *channel)
{
    bool ok = false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (!s_scanning && idx >= 1 && idx <= s_total) {
        memcpy(bssid, s_results[idx - 1].bssid, 6);
        *channel = s_results[idx - 1].primary;
        ok = true;
    }
    xSemaphoreGive(s_lock);
    return ok;
}

void wifi_cmd_scan(void)
{
    if (arbiter_acquire(PHY_OWNER_WIFI, scan_teardown) != ESP_OK) {
        char msg[48];
        snprintf(msg, sizeof msg, "radio in use by %s", arbiter_owner_name(arbiter_owner()));
        ocp_emit_error(OCP_ERR_BUSY, msg);
        return;
    }

    wifi_scan_config_t cfg = wifi_passive_scan_config();

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_scanning = true;
    s_aborting = false;
    s_started_us = esp_timer_get_time();
    xSemaphoreGive(s_lock);

    esp_err_t err = esp_wifi_scan_start(&cfg, false);
    if (err != ESP_OK) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_scanning = false;
        xSemaphoreGive(s_lock);
        arbiter_release(PHY_OWNER_WIFI);
        ocp_emit_error(OCP_ERR_HWFAULT, esp_err_to_name(err));
    }
    /* Otherwise the reply is emitted by on_scan_done. */
}

void wifi_cmd_show_results(int argc, char **argv)
{
    uint32_t first = 1;
    if (argc > 1) {
        if (!ocp_parse_u32(argv[1], &first) || first < 1 || first > 65535) {
            ocp_emit_error(OCP_ERR_BADARG, "first must be a positive index");
            return;
        }
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_scanning) {
        xSemaphoreGive(s_lock);
        ocp_emit_error(OCP_ERR_BUSY, "scan in progress");
        return;
    }
    if (s_total && first > s_total) {
        xSemaphoreGive(s_lock);
        ocp_emit_error(OCP_ERR_BADARG, "first is past the stored results");
        return;
    }
    emit_page((uint16_t)first, false);
    xSemaphoreGive(s_lock);
}
