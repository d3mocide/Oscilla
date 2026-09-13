/*
 * wifi_inspect.c — see wifi_inspect.h.
 *
 * The promiscuous callback runs in the Wi-Fi driver task: it only matches the
 * BSSID, parses (bounded, beacon_parse.c) and records. The reply is emitted
 * by the capture timer or by `stop`, never from the driver task. finish() is
 * idempotent, so the timer and stop can race safely.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_inspect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "beacon_parse.h"
#include "ocp.h"
#include "ocp_frame.h"
#include "radio_arbiter.h"
#include "wifi_recon.h"

static const char *TAG = "inspect";

static SemaphoreHandle_t s_lock;
static esp_timer_handle_t s_timer;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* Shared with the driver-task callback: guarded by s_mux. */
static bool s_active;
static uint8_t s_bssid[6];
static uint16_t s_beacons;
static int8_t s_rssi;
static beacon_info_t s_last;

/* Owned by the command path: guarded by s_lock. */
static unsigned s_idx;
static uint8_t s_channel;

static void on_frame(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *pkt = buf;
    size_t len = pkt->rx_ctrl.sig_len;
    if (len < 4) return;
    len -= 4;                                        /* FCS */

    beacon_info_t bi;
    if (beacon_parse(pkt->payload, len, &bi) != BEACON_OK) return;

    bool enough = false;
    portENTER_CRITICAL(&s_mux);
    if (s_active && memcmp(bi.bssid, s_bssid, 6) == 0) {
        s_beacons++;
        s_rssi = pkt->rx_ctrl.rssi;
        s_last = bi;
        enough = s_beacons >= INSPECT_ENOUGH;
    }
    portEXIT_CRITICAL(&s_mux);

    if (enough) {                                    /* end early: re-arm for now */
        esp_timer_stop(s_timer);
        esp_timer_start_once(s_timer, 1000);
    }
}

static void finish(bool aborted)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);

    portENTER_CRITICAL(&s_mux);
    bool was_active = s_active;
    s_active = false;
    uint16_t beacons = s_beacons;
    int8_t rssi = s_rssi;
    beacon_info_t bi = s_last;
    portEXIT_CRITICAL(&s_mux);

    if (!was_active) {
        xSemaphoreGive(s_lock);
        return;
    }

    esp_timer_stop(s_timer);
    esp_wifi_set_promiscuous(false);

    const uint8_t *b = s_bssid;
    ocp_emit_begin(OCP_MARK_INSPECT, "%s=%u %s=%02x:%02x:%02x:%02x:%02x:%02x %s=%u %s=%s%s",
                   OCP_K_IDX, s_idx, OCP_K_BSSID, b[0], b[1], b[2], b[3], b[4], b[5],
                   OCP_K_CH, s_channel, OCP_K_BAND,
                   s_channel <= 14 ? OCP_BAND_LABEL_24 : OCP_BAND_LABEL_5,
                   aborted ? " " OCP_K_ABORTED "=1" : "");
    if (beacons == 0) {
        ocp_emit_row(OCP_MARK_INSPECT, "%s=0", OCP_K_BEACONS);
    } else {
        bool rsn = bi.has_rsn && !bi.rsn_malformed;
        ocp_emit_row(OCP_MARK_INSPECT, "%s=%u %s=%d %s=%d %s=%d %s=%d %s=%llu %s=%u",
                     OCP_K_BEACONS, beacons, OCP_K_RSSI, rssi, OCP_K_RSN, rsn,
                     OCP_K_MFP_CAPABLE, rsn && bi.mfp_capable,
                     OCP_K_MFP_REQUIRED, rsn && bi.mfp_required,
                     OCP_K_UPTIME_S, (unsigned long long)(bi.tsf_us / 1000000ULL),
                     OCP_K_INTERVAL_MS, (unsigned)((bi.interval_tu * 1024u + 500u) / 1000u));
    }
    ocp_emit_end(OCP_MARK_INSPECT);

    arbiter_release(PHY_OWNER_WIFI);
    xSemaphoreGive(s_lock);
}

static void on_window_end(void *arg) { (void)arg; finish(false); }

static void inspect_teardown(void) { finish(true); }

esp_err_t wifi_inspect_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    const esp_timer_create_args_t args = { .callback = on_window_end, .name = "inspect" };
    esp_err_t err = esp_timer_create(&args, &s_timer);
    if (err != ESP_OK) return err;

    const wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
    if ((err = esp_wifi_set_promiscuous_filter(&filter)) != ESP_OK) return err;
    return esp_wifi_set_promiscuous_rx_cb(on_frame);
}

void wifi_cmd_inspect(int argc, char **argv)
{
    char *end = NULL;
    unsigned long idx = strtoul(argv[1], &end, 10);
    uint8_t bssid[6], channel = 0;
    (void)argc;

    if (!*argv[1] || *end || idx < 1 || !wifi_recon_lookup((unsigned)idx, bssid, &channel)) {
        ocp_emit_error(OCP_ERR_BADARG, "idx is not a stored scan result");
        return;
    }
    if (arbiter_acquire(PHY_OWNER_WIFI, inspect_teardown) != ESP_OK) {
        char msg[48];
        snprintf(msg, sizeof msg, "radio in use by %s", arbiter_owner_name(arbiter_owner()));
        ocp_emit_error(OCP_ERR_BUSY, msg);
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_idx = (unsigned)idx;
    s_channel = channel;
    portENTER_CRITICAL(&s_mux);
    memcpy(s_bssid, bssid, 6);
    s_beacons = 0;
    s_rssi = 0;
    memset(&s_last, 0, sizeof s_last);
    s_active = true;
    portEXIT_CRITICAL(&s_mux);

    esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous(true);
    if (err == ESP_OK) err = esp_timer_start_once(s_timer, (uint64_t)INSPECT_WINDOW_MS * 1000);
    xSemaphoreGive(s_lock);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "capture start on ch %u: %s", channel, esp_err_to_name(err));
        portENTER_CRITICAL(&s_mux);
        s_active = false;
        portEXIT_CRITICAL(&s_mux);
        esp_wifi_set_promiscuous(false);
        arbiter_release(PHY_OWNER_WIFI);
        ocp_emit_error(OCP_ERR_HWFAULT, esp_err_to_name(err));
    }
}
