/*
 * wifi_sniff.c — see wifi_sniff.h.
 *
 * Tracking-table logic (address extraction, dedup, overflow) lives in
 * sniff_track.c, pure C and host-fuzzed; this file is IDF glue: the
 * promiscuous callback, the channel hopper, the arbiter, and OCP framing.
 *
 * Locking: one mutex (s_lock) guards the track table. The promiscuous
 * callback (Wi-Fi driver task, not ISR — same context wifi_inspect.c's
 * on_frame runs in) takes it per frame, same as a command handler dumping a
 * table; holding it across a full-table emit mirrors wifi_recon.c's
 * emit_page(), already an accepted pattern here. s_lock is always taken
 * before the frame lock inside ocp_frame, never the reverse.
 *
 * Channel hop set: wifi_channels.h (shared with wifi_deauth.c, wifi_spectrum.c).
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_sniff.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ocp.h"
#include "ocp_frame.h"
#include "ocp_text.h"
#include "probe_parse.h"
#include "radio_arbiter.h"
#include "sniff_track.h"
#include "wifi_channels.h"

static const char *TAG = "sniff";

static const uint8_t *k_channels;
static size_t k_num_channels;

static SemaphoreHandle_t s_lock;
static esp_timer_handle_t s_hop_timer;

static sniff_track_t *s_track;
static uint8_t s_chan_idx;
static uint32_t s_total_pkts;
static int64_t s_started_us;

/* Runs in the Wi-Fi driver task (see wifi_inspect.c's on_frame). Filter is
 * MGMT|DATA (no *_AMPDU): aggregated data frames are not this simple a
 * header and are silently missed rather than mis-parsed. */
static void on_frame(void *buf, wifi_promiscuous_pkt_type_t type)
{
    const wifi_promiscuous_pkt_t *pkt = buf;
    if (pkt->rx_ctrl.sig_len < 4) return;
    size_t len = pkt->rx_ctrl.sig_len - 4;             /* FCS */
    const uint8_t *frame = pkt->payload;
    int8_t rssi = pkt->rx_ctrl.rssi;

    int kind = 0;                                       /* 0=none, 1=client, 2=probe */
    sniff_client_t new_client;
    sniff_probe_t new_probe;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_total_pkts++;
    uint8_t ch = k_channels[s_chan_idx];

    if (type == WIFI_PKT_MGMT) {
        probe_req_info_t pi;
        if (probe_req_parse(frame, len, &pi) == PROBE_OK) {
            uint8_t slen = pi.has_ssid ? pi.ssid_len : 0;
            if (sniff_track_probe(s_track, pi.mac, pi.ssid, slen, rssi, &new_probe)) kind = 2;
        }
    } else if (type == WIFI_PKT_DATA) {
        if (sniff_track_data_frame(s_track, frame, len, ch, rssi, &new_client)) kind = 1;
    }
    xSemaphoreGive(s_lock);

    if (kind == 1) {
        const sniff_client_t *c = &new_client;
        ocp_event_record_t event = { .kind = OCP_EVENT_RECORD_CLIENT };
        memcpy(event.data.client.bssid, c->bssid, sizeof c->bssid);
        memcpy(event.data.client.mac, c->mac, sizeof c->mac);
        event.data.client.channel = c->channel;
        event.data.client.rssi = c->rssi;
        (void)ocp_event_submit(&event);
    } else if (kind == 2) {
        const sniff_probe_t *p = &new_probe;
        ocp_event_record_t event = { .kind = OCP_EVENT_RECORD_PROBE };
        memcpy(event.data.probe.mac, p->mac, sizeof p->mac);
        memcpy(event.data.probe.ssid, p->ssid, sizeof p->ssid);
        event.data.probe.ssid_len = p->ssid_len;
        event.data.probe.rssi = p->rssi;
        (void)ocp_event_submit(&event);
    }
}

/* Timer task: advance the hop, report the channel just finished. */
static void on_hop(void *arg)
{
    (void)arg;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint8_t finished_ch = k_channels[s_chan_idx];
    uint32_t pkts = s_total_pkts;
    s_chan_idx = (uint8_t)((s_chan_idx + 1) % k_num_channels);
    uint8_t next_ch = k_channels[s_chan_idx];
    xSemaphoreGive(s_lock);

    esp_err_t err = esp_wifi_set_channel(next_ch, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) ESP_LOGE(TAG, "hop to ch %u: %s", next_ch, esp_err_to_name(err));

    ocp_event_record_t event = { .kind = OCP_EVENT_RECORD_SNIFF };
    event.data.sniff.channel = finished_ch;
    event.data.sniff.packets = pkts;
    (void)ocp_event_submit(&event);
}

/* Dispatch task, via arbiter_stop_all(). No frame to close: [SNIFF] already
 * returned at start, so `stop` just silences the stream (OCP-SPEC §10.4). */
static void sniff_teardown(void)
{
    esp_timer_stop(s_hop_timer);
    esp_wifi_set_promiscuous(false);
    arbiter_release(PHY_OWNER_WIFI);
}

esp_err_t wifi_sniff_init(void)
{
    k_channels = wifi_channels(&k_num_channels);
    s_track = heap_caps_calloc(1, sizeof *s_track,
                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_track) {
        s_track = heap_caps_calloc(1, sizeof *s_track,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    s_lock = xSemaphoreCreateMutex();
    if (!s_track || !s_lock) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "sniff table in %s RAM", esp_ptr_external_ram(s_track) ? "PSRAM" : "internal");
    const esp_timer_create_args_t args = { .callback = on_hop, .name = "sniff_hop" };
    return esp_timer_create(&args, &s_hop_timer);
}

void wifi_cmd_start_sniffer(void)
{
    if (arbiter_acquire(PHY_OWNER_WIFI, sniff_teardown) != ESP_OK) {
        char msg[48];
        snprintf(msg, sizeof msg, "radio in use by %s", arbiter_owner_name(arbiter_owner()));
        ocp_emit_error(OCP_ERR_BUSY, msg);
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    sniff_track_reset(s_track);
    s_total_pkts = 0;
    s_chan_idx = 0;
    s_started_us = esp_timer_get_time();
    xSemaphoreGive(s_lock);

    const wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA,
    };
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
    ocp_emit_compact(OCP_MARK_SNIFF, "%s=%u", OCP_K_CH, ch);
}

void wifi_cmd_show_clients(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint32_t elapsed = (uint32_t)((esp_timer_get_time() - s_started_us) / 1000);
    unsigned n = s_track->client_count;
    ocp_emit_begin(OCP_MARK_CLIENTS, "%s=%u %s=%u %s=%u", OCP_K_COUNT, n, OCP_K_TOTAL, n, OCP_K_ELAPSED_MS, elapsed);
    for (unsigned i = 0; i < n; i++) {
        const sniff_client_t *c = &s_track->clients[i];
        ocp_emit_row(OCP_MARK_CLIENTS,
                     "\"%02x:%02x:%02x:%02x:%02x:%02x\",\"%02x:%02x:%02x:%02x:%02x:%02x\",\"%u\",\"%s\",\"%d\",\"%u\"",
                     c->bssid[0], c->bssid[1], c->bssid[2], c->bssid[3], c->bssid[4], c->bssid[5],
                     c->mac[0], c->mac[1], c->mac[2], c->mac[3], c->mac[4], c->mac[5],
                     c->channel, c->channel <= 14 ? OCP_BAND_LABEL_24 : OCP_BAND_LABEL_5,
                     c->rssi, (unsigned)c->pkts);
    }
    ocp_emit_end(OCP_MARK_CLIENTS);
    xSemaphoreGive(s_lock);
}

void wifi_cmd_show_probes(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint32_t elapsed = (uint32_t)((esp_timer_get_time() - s_started_us) / 1000);
    unsigned n = s_track->probe_count;
    ocp_emit_begin(OCP_MARK_PROBES, "%s=%u %s=%u %s=%u", OCP_K_COUNT, n, OCP_K_TOTAL, n, OCP_K_ELAPSED_MS, elapsed);
    for (unsigned i = 0; i < n; i++) {
        const sniff_probe_t *p = &s_track->probes[i];
        char ssid[4 * 32 + 3];
        ocp_escape_field(p->ssid, p->ssid_len, ssid, sizeof ssid);
        ocp_emit_row(OCP_MARK_PROBES, "\"%02x:%02x:%02x:%02x:%02x:%02x\",%s,\"%d\",\"%u\"",
                     p->mac[0], p->mac[1], p->mac[2], p->mac[3], p->mac[4], p->mac[5],
                     ssid, p->rssi, (unsigned)p->pkts);
    }
    ocp_emit_end(OCP_MARK_PROBES);
    xSemaphoreGive(s_lock);
}
