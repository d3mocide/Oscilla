/*
 * wifi_spectrum.c — see wifi_spectrum.h.
 *
 * Both engines here only count frames (WIFI_PROMIS_FILTER_MASK_ALL, no
 * payload ever read), so there is no address/IE parsing and no bounds risk
 * to fuzz — unlike wifi_sniff.c/wifi_deauth.c, this file has no pure-C
 * counterpart to host-test.
 *
 * Locking: one mutex (s_lock) guards whichever counter the active mode is
 * using. Only one of channel_view / packet_monitor runs at a time (the
 * arbiter), but each still gets its own state and its own timer so
 * `stop`-then-restart-the-other never has to reset the first one's leftovers.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_spectrum.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ocp.h"
#include "ocp_parse.h"
#include "ocp_frame.h"
#include "radio_arbiter.h"
#include "wifi_channels.h"

static const char *TAG = "spectrum";

static const uint8_t *k_channels;
static size_t k_num_channels;

static SemaphoreHandle_t s_lock;

/* --- channel_view: hops, one live reading per channel per lap ------------ */

static esp_timer_handle_t s_cv_timer;
static uint8_t s_cv_idx;
static uint32_t s_cv_pkts;   /* frames seen during the dwell in progress */

static void on_frame_cv(void *buf, wifi_promiscuous_pkt_type_t type)
{
    (void)buf; (void)type;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_cv_pkts++;
    xSemaphoreGive(s_lock);
}

static void on_cv_hop(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint8_t idx = s_cv_idx;
    uint32_t pkts = s_cv_pkts;
    s_cv_pkts = 0;
    s_cv_idx = (uint8_t)((idx + 1) % k_num_channels);
    uint8_t next_ch = k_channels[s_cv_idx];
    xSemaphoreGive(s_lock);

    ocp_event_record_t event = { .kind = OCP_EVENT_RECORD_CHAN };
    event.data.chan.channel = k_channels[idx];
    event.data.chan.packets = pkts;
    (void)ocp_event_submit(&event);

    esp_err_t err = esp_wifi_set_channel(next_ch, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) ESP_LOGE(TAG, "hop to ch %u: %s", next_ch, esp_err_to_name(err));
}

static void cv_teardown(void)
{
    esp_timer_stop(s_cv_timer);
    esp_wifi_set_promiscuous(false);
    arbiter_release(PHY_OWNER_WIFI);
}

void wifi_cmd_channel_view(void)
{
    if (arbiter_acquire(PHY_OWNER_WIFI, cv_teardown) != ESP_OK) {
        char msg[48];
        snprintf(msg, sizeof msg, "radio in use by %s", arbiter_owner_name(arbiter_owner()));
        ocp_emit_error(OCP_ERR_BUSY, msg);
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_cv_idx = 0;
    s_cv_pkts = 0;
    xSemaphoreGive(s_lock);

    const wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL };
    uint8_t ch = k_channels[0];
    esp_err_t err = esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous_filter(&filter);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous_rx_cb(on_frame_cv);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous(true);
    if (err == ESP_OK) err = esp_timer_start_periodic(s_cv_timer, (uint64_t)WIFI_CHAN_DWELL_MS * 1000);

    if (err != ESP_OK) {
        esp_wifi_set_promiscuous(false);
        arbiter_release(PHY_OWNER_WIFI);
        ocp_emit_error(OCP_ERR_HWFAULT, esp_err_to_name(err));
        return;
    }
    ocp_emit_compact(OCP_MARK_CHAN, "%s=%u %s=%u", OCP_K_CH, ch, OCP_K_COUNT, (unsigned)k_num_channels);
}

/* --- packet_monitor: one fixed channel, packets/s ------------------------ */

static esp_timer_handle_t s_pm_timer;
static uint8_t s_pm_channel;
static uint32_t s_pm_pkts;

static void on_frame_pm(void *buf, wifi_promiscuous_pkt_type_t type)
{
    (void)buf; (void)type;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_pm_pkts++;
    xSemaphoreGive(s_lock);
}

static void on_pm_tick(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint32_t pkts = s_pm_pkts;
    s_pm_pkts = 0;
    xSemaphoreGive(s_lock);

    ocp_event_record_t event = { .kind = OCP_EVENT_RECORD_CHAN };
    event.data.chan.channel = s_pm_channel;
    event.data.chan.packets = pkts;
    (void)ocp_event_submit(&event);
}

static void pm_teardown(void)
{
    esp_timer_stop(s_pm_timer);
    esp_wifi_set_promiscuous(false);
    arbiter_release(PHY_OWNER_WIFI);
}

/* True if `ch` is one of the channels this build actually hops (D-14: not
 * every 5 GHz channel is offered, so a stray value is rejected up front
 * rather than handed to esp_wifi_set_channel to fail on its own). */
static bool channel_supported(unsigned long ch)
{
    for (size_t i = 0; i < k_num_channels; i++) {
        if (k_channels[i] == ch) return true;
    }
    return false;
}

void wifi_cmd_packet_monitor(int argc, char **argv)
{
    (void)argc;
    uint32_t ch = 0;
    if (!ocp_parse_u32(argv[1], &ch) || ch < 1 || ch > 255 || !channel_supported(ch)) {
        ocp_emit_error(OCP_ERR_BADARG, "channel is not in the supported hop list");
        return;
    }

    if (arbiter_acquire(PHY_OWNER_WIFI, pm_teardown) != ESP_OK) {
        char msg[48];
        snprintf(msg, sizeof msg, "radio in use by %s", arbiter_owner_name(arbiter_owner()));
        ocp_emit_error(OCP_ERR_BUSY, msg);
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_pm_channel = (uint8_t)ch;
    s_pm_pkts = 0;
    xSemaphoreGive(s_lock);

    const wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL };
    esp_err_t err = esp_wifi_set_channel((uint8_t)ch, WIFI_SECOND_CHAN_NONE);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous_filter(&filter);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous_rx_cb(on_frame_pm);
    if (err == ESP_OK) err = esp_wifi_set_promiscuous(true);
    if (err == ESP_OK) err = esp_timer_start_periodic(s_pm_timer, (uint64_t)PACKET_MONITOR_INTERVAL_MS * 1000);

    if (err != ESP_OK) {
        esp_wifi_set_promiscuous(false);
        arbiter_release(PHY_OWNER_WIFI);
        ocp_emit_error(OCP_ERR_HWFAULT, esp_err_to_name(err));
        return;
    }
    ocp_emit_compact(OCP_MARK_CFG, "%s=%u", OCP_K_CH, (unsigned)ch);
}

esp_err_t wifi_spectrum_init(void)
{
    k_channels = wifi_channels(&k_num_channels);
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;

    const esp_timer_create_args_t cv_args = { .callback = on_cv_hop, .name = "cv_hop" };
    esp_err_t err = esp_timer_create(&cv_args, &s_cv_timer);
    if (err != ESP_OK) return err;

    const esp_timer_create_args_t pm_args = { .callback = on_pm_tick, .name = "pm_tick" };
    return esp_timer_create(&pm_args, &s_pm_timer);
}
