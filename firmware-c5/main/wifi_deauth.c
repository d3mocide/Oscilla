/*
 * wifi_deauth.c — see wifi_deauth.h.
 *
 * Locking: one mutex (s_lock) guards the hop index and session counter, same
 * discipline as wifi_sniff.c. The promiscuous callback runs in the Wi-Fi
 * driver task (see wifi_inspect.c's on_frame), not an ISR.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_deauth.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "deauth_parse.h"
#include "ocp.h"
#include "ocp_frame.h"
#include "radio_arbiter.h"
#include "wifi_channels.h"

static const char *TAG = "deauth";

static const uint8_t *k_channels;
static size_t k_num_channels;

static SemaphoreHandle_t s_lock;
static esp_timer_handle_t s_hop_timer;
static uint8_t s_chan_idx;
static uint32_t s_total;

/* Runs in the Wi-Fi driver task. Every deauth/disassoc is reported — unlike
 * the sniffer's new-pairing-only events, occurrences (especially a sudden
 * burst) are themselves the signal an operator watches for. */
static void on_frame(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *pkt = buf;
    if (pkt->rx_ctrl.sig_len < 4) return;
    size_t len = pkt->rx_ctrl.sig_len - 4;   /* FCS */

    deauth_info_t di;
    if (deauth_parse(pkt->payload, len, &di) != DEAUTH_OK) return;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint32_t total = ++s_total;
    uint8_t ch = k_channels[s_chan_idx];
    xSemaphoreGive(s_lock);

    ocp_event_record_t event = { .kind = OCP_EVENT_RECORD_DEAUTH };
    memcpy(event.data.deauth.bssid, di.bssid, sizeof di.bssid);
    memcpy(event.data.deauth.mac, di.dest, sizeof di.dest);
    event.data.deauth.reason = di.reason;
    event.data.deauth.is_disassoc = di.is_disassoc;
    event.data.deauth.rssi = pkt->rx_ctrl.rssi;
    event.data.deauth.channel = ch;
    event.data.deauth.count = total;
    (void)ocp_event_submit(&event);
}

/* Timer task: just advance the hop. No periodic heartbeat event — unlike
 * the sniffer, silence here is the good outcome, not something to narrate. */
static void on_hop(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_chan_idx = (uint8_t)((s_chan_idx + 1) % k_num_channels);
    uint8_t next_ch = k_channels[s_chan_idx];
    xSemaphoreGive(s_lock);

    esp_err_t err = esp_wifi_set_channel(next_ch, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) ESP_LOGE(TAG, "hop to ch %u: %s", next_ch, esp_err_to_name(err));
}

/* Dispatch task, via arbiter_stop_all(). No frame to close: [CFG] already
 * returned at start, so `stop` just silences the stream. */
static void deauth_teardown(void)
{
    esp_timer_stop(s_hop_timer);
    esp_wifi_set_promiscuous(false);
    arbiter_release(PHY_OWNER_WIFI);
}

esp_err_t wifi_deauth_init(void)
{
    k_channels = wifi_channels(&k_num_channels);
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    const esp_timer_create_args_t args = { .callback = on_hop, .name = "deauth_hop" };
    return esp_timer_create(&args, &s_hop_timer);
}

void wifi_cmd_deauth_detector(void)
{
    if (arbiter_acquire(PHY_OWNER_WIFI, deauth_teardown) != ESP_OK) {
        char msg[48];
        snprintf(msg, sizeof msg, "radio in use by %s", arbiter_owner_name(arbiter_owner()));
        ocp_emit_error(OCP_ERR_BUSY, msg);
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_total = 0;
    s_chan_idx = 0;
    xSemaphoreGive(s_lock);

    const wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
    uint8_t ch = k_channels[0];
    esp_err_t err = esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous_filter(&filter);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous_rx_cb(on_frame);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous(true);
    if (err == ESP_OK) err = esp_timer_start_periodic(s_hop_timer, (uint64_t)WIFI_CHAN_DWELL_MS * 1000);

    if (err != ESP_OK) {
        esp_wifi_set_promiscuous(false);
        arbiter_release(PHY_OWNER_WIFI);
        ocp_emit_error(OCP_ERR_HWFAULT, esp_err_to_name(err));
        return;
    }
    ocp_emit_compact(OCP_MARK_CFG, "%s=%u", OCP_K_CH, ch);
}
