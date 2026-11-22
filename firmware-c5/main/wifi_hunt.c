/*
 * wifi_hunt.c — see wifi_hunt.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_hunt.h"

#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "hunt_filter.h"
#include "ocp.h"
#include "ocp_frame.h"
#include "ocp_parse.h"
#include "radio_arbiter.h"
#include "wifi_channels.h"

static SemaphoreHandle_t s_lock;
static bool s_running;
static uint8_t s_target_mac[6];
static uint8_t s_channel;
static hunt_rate_limit_t s_rl;

static bool channel_in_active_region(uint8_t ch)
{
    const ocp_wifi_region_t *r = wifi_region();
    for (size_t i = 0; i < r->count; i++) {
        if (r->channels[i] == ch) return true;
    }
    return false;
}

static void on_frame(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;
    const wifi_promiscuous_pkt_t *pkt = buf;
    if (pkt->rx_ctrl.sig_len < 28) return;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool emit = s_running &&
                hunt_wifi_frame_matches(pkt->payload, pkt->rx_ctrl.sig_len - 4, s_target_mac) &&
                hunt_rate_limit_allow(&s_rl, (uint32_t)(esp_timer_get_time() / 1000ULL));
    uint8_t ch = s_channel;
    uint8_t mac[6];
    if (emit) memcpy(mac, s_target_mac, 6);
    xSemaphoreGive(s_lock);

    if (!emit) return;
    ocp_event_record_t ev = { .kind = OCP_EVENT_RECORD_HUNT };
    memcpy(ev.data.hunt.mac, mac, 6);
    ev.data.hunt.rssi = pkt->rx_ctrl.rssi;
    ev.data.hunt.channel = ch;
    (void)ocp_event_submit(&ev);
}

static void teardown(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_running = false;
    xSemaphoreGive(s_lock);
    (void)esp_wifi_set_promiscuous(false);
    arbiter_release(PHY_OWNER_WIFI);
}

esp_err_t wifi_hunt_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    return s_lock ? ESP_OK : ESP_ERR_NO_MEM;
}

void wifi_cmd_hunt(int argc, char **argv)
{
    uint8_t mac[6];
    uint32_t ch = 0;
    if (argc < 3 || !hunt_parse_mac(argv[1], mac) || !ocp_parse_u32(argv[2], &ch) ||
        ch > 255u || !channel_in_active_region((uint8_t)ch)) {
        ocp_emit_error(OCP_ERR_BADARG, "hunt_wifi requires valid mac and active-region ch");
        return;
    }

    if (arbiter_acquire(PHY_OWNER_WIFI, teardown) != ESP_OK) {
        char msg[48];
        snprintf(msg, sizeof msg, "radio in use by %s", arbiter_owner_name(arbiter_owner()));
        ocp_emit_error(OCP_ERR_BUSY, msg);
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    memcpy(s_target_mac, mac, 6);
    s_channel = (uint8_t)ch;
    hunt_rate_limit_reset(&s_rl, HUNT_MIN_INTERVAL_MS);
    s_running = false;
    xSemaphoreGive(s_lock);

    const wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA,
    };
    esp_err_t err = esp_wifi_set_channel((uint8_t)ch, WIFI_SECOND_CHAN_NONE);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous_filter(&filter);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous_rx_cb(on_frame);
    if (err == ESP_OK) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_running = true;
        xSemaphoreGive(s_lock);
        err = esp_wifi_set_promiscuous(true);
    }

    if (err != ESP_OK) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_running = false;
        xSemaphoreGive(s_lock);
        (void)esp_wifi_set_promiscuous(false);
        arbiter_release(PHY_OWNER_WIFI);
        ocp_emit_error(OCP_ERR_HWFAULT, esp_err_to_name(err));
        return;
    }

    ocp_emit_compact(OCP_MARK_CFG, "%s=%s %s=%02x:%02x:%02x:%02x:%02x:%02x %s=%u",
                     OCP_K_MODE, OCP_EVT_KIND_HUNT,
                     OCP_K_MAC, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                     OCP_K_CH, (unsigned)ch);
}
