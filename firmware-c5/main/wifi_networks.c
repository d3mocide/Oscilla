/*
 * wifi_networks.c — continuous passive beacon/probe-response discovery.
 *
 * The callback only parses bounded management frames and copies a newly
 * discovered row; channel hopping and OCP emission happen outside it.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_networks.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "beacon_parse.h"
#include "ocp.h"
#include "ocp_frame.h"
#include "ocp_text.h"
#include "radio_arbiter.h"
#include "wifi_channels.h"
#include "wifi_network_table.h"

static const char *TAG = "wifi_networks";

static const uint8_t *k_channels;
static size_t k_num_channels;
static SemaphoreHandle_t s_lock;
static QueueHandle_t s_event_queue;
static esp_timer_handle_t s_hop_timer;
static wifi_network_table_t s_table;
static uint8_t s_chan_idx;
static uint32_t s_session;
static bool s_running;

typedef struct {
    uint32_t session;
    wifi_network_t row;
} network_event_t;

static void emit_network_event(const wifi_network_t *row)
{
    char ssid[4 * 32 + 3];
    ocp_escape_field(row->ssid, row->has_ssid ? row->ssid_len : 0, ssid, sizeof ssid);
    ocp_emit_event(OCP_EVT_KIND_NETWORK,
                   "%s=%02x:%02x:%02x:%02x:%02x:%02x %s=%s %s=%u %s=%s %s=%d %s=%d %s=%d %s=%d %s=%d %s=%u",
                   OCP_K_BSSID, row->bssid[0], row->bssid[1], row->bssid[2],
                   row->bssid[3], row->bssid[4], row->bssid[5],
                   OCP_K_SSID, ssid, OCP_K_CH, (unsigned)row->channel, OCP_K_BAND,
                   row->channel <= 14 ? OCP_BAND_LABEL_24 : OCP_BAND_LABEL_5,
                   OCP_K_RSSI, row->rssi, OCP_K_PRIVACY, row->privacy, OCP_K_RSN, row->has_rsn,
                   OCP_K_MFP_CAPABLE, row->mfp_capable,
                   OCP_K_MFP_REQUIRED, row->mfp_required,
                   OCP_K_INTERVAL_MS, (unsigned)row->interval_ms);
}

/* UART writes are not allowed to stall the Wi-Fi driver task. */
static void network_event_task(void *arg)
{
    (void)arg;
    network_event_t event;
    for (;;) {
        if (xQueueReceive(s_event_queue, &event, portMAX_DELAY) != pdTRUE) continue;

        xSemaphoreTake(s_lock, portMAX_DELAY);
        bool current = s_running && event.session == s_session;
        xSemaphoreGive(s_lock);
        if (current) emit_network_event(&event.row);
    }
}

static void on_frame(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *pkt = buf;
    if (pkt->rx_ctrl.sig_len < 4) return;

    beacon_info_t info;
    if (beacon_parse(pkt->payload, pkt->rx_ctrl.sig_len - 4, &info) != BEACON_OK) return;

    wifi_network_t row;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint8_t channel = k_channels[s_chan_idx];
    bool is_new = wifi_network_table_upsert(&s_table, &info, channel,
                                            pkt->rx_ctrl.rssi, &row);
    xSemaphoreGive(s_lock);
    if (!is_new) return;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool running = s_running;
    network_event_t event = { .session = s_session, .row = row };
    xSemaphoreGive(s_lock);
    if (running) (void)xQueueSend(s_event_queue, &event, 0);
}

static void on_hop(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_chan_idx = (uint8_t)((s_chan_idx + 1) % k_num_channels);
    uint8_t next = k_channels[s_chan_idx];
    xSemaphoreGive(s_lock);

    esp_err_t err = esp_wifi_set_channel(next, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) ESP_LOGE(TAG, "hop to ch %u: %s", next, esp_err_to_name(err));
}

static void teardown(void)
{
    esp_timer_stop(s_hop_timer);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_running = false;
    xSemaphoreGive(s_lock);
    esp_wifi_set_promiscuous(false);
    arbiter_release(PHY_OWNER_WIFI);
}

esp_err_t wifi_networks_init(void)
{
    k_channels = wifi_channels(&k_num_channels);
    s_lock = xSemaphoreCreateMutex();
    s_event_queue = xQueueCreate(64, sizeof(network_event_t));
    if (!s_lock || !s_event_queue) return ESP_ERR_NO_MEM;
    const esp_timer_create_args_t args = { .callback = on_hop, .name = "network_hop" };
    esp_err_t err = esp_timer_create(&args, &s_hop_timer);
    if (err != ESP_OK) return err;
    return xTaskCreate(network_event_task, "wifi_net_evt", 3072, NULL, 4, NULL) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

void wifi_cmd_start_network_scan(void)
{
    if (arbiter_acquire(PHY_OWNER_WIFI, teardown) != ESP_OK) {
        char msg[48];
        snprintf(msg, sizeof msg, "radio in use by %s", arbiter_owner_name(arbiter_owner()));
        ocp_emit_error(OCP_ERR_BUSY, msg);
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    wifi_network_table_reset(&s_table);
    s_chan_idx = 0;
    s_session++;
    if (s_session == 0) s_session = 1;
    s_running = false;
    xQueueReset(s_event_queue);
    xSemaphoreGive(s_lock);

    const wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
    uint8_t channel = k_channels[0];
    esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous_filter(&filter);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous_rx_cb(on_frame);
    if (err == ESP_OK) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_running = true;
        xSemaphoreGive(s_lock);
    }
    if (err == ESP_OK) err = esp_wifi_set_promiscuous(true);
    if (err == ESP_OK) err = esp_timer_start_periodic(s_hop_timer, (uint64_t)WIFI_CHAN_DWELL_MS * 1000);

    if (err != ESP_OK) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_running = false;
        xSemaphoreGive(s_lock);
        esp_wifi_set_promiscuous(false);
        arbiter_release(PHY_OWNER_WIFI);
        ocp_emit_error(OCP_ERR_HWFAULT, esp_err_to_name(err));
        return;
    }
    ocp_emit_compact(OCP_MARK_CFG, "%s=%u", OCP_K_CH, channel);
}
