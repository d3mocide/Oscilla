/*
 * wifi_wardrive.c — see wifi_wardrive.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "wifi_wardrive.h"

#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "beacon_parse.h"
#include "ble_recon.h"
#include "ocp.h"
#include "ocp_frame.h"
#include "ocp_parse.h"
#include "radio_arbiter.h"
#include "wardrive_cache.h"
#include "wifi_channels.h"

static const char *TAG = "wardrive";

#define WARDRIVE_WIFI_CACHE_CAP    256u
#define WARDRIVE_BLE_CACHE_CAP     128u
#define WARDRIVE_WIFI_DWELL_DEF    200u
#define WARDRIVE_WIFI_DWELL_MIN    50u
#define WARDRIVE_WIFI_DWELL_MAX    1000u
#define WARDRIVE_BLE_DWELL_DEF     1500u
#define WARDRIVE_BLE_DWELL_MIN     250u
#define WARDRIVE_BLE_DWELL_MAX     5000u
#define WARDRIVE_TTL_S_DEF         30u
#define WARDRIVE_TTL_S_MIN         5u
#define WARDRIVE_TTL_S_MAX         300u
#define WARDRIVE_STOP_WAIT_MS      600u

static SemaphoreHandle_t s_lock;
static wardrive_cache_entry_t *s_wifi_buf;
static wardrive_cache_entry_t *s_ble_buf;
static wardrive_cache_t s_wifi_cache;
static wardrive_cache_t s_ble_cache;

static volatile bool s_stop_req;
static volatile bool s_task_idle = true;
static volatile uint8_t s_cur_channel;
static bool s_ble_enabled;
static uint32_t s_wifi_dwell_ms;
static uint32_t s_ble_dwell_ms;
static uint32_t s_ttl_ms;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void on_wifi_frame(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (s_stop_req || type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t *pkt = buf;
    if (pkt->rx_ctrl.sig_len < 4) return;

    beacon_info_t info;
    if (beacon_parse(pkt->payload, pkt->rx_ctrl.sig_len - 4, &info) != BEACON_OK) return;
    if ((info.bssid[0] & 0x01u) != 0) return;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool emit = !s_stop_req &&
                wardrive_cache_should_emit(&s_wifi_cache, info.bssid,
                                           pkt->rx_ctrl.rssi, now_ms());
    xSemaphoreGive(s_lock);
    if (!emit) return;

    bool has_rsn = info.has_rsn && !info.rsn_malformed;
    ocp_event_record_t event = { .kind = OCP_EVENT_RECORD_NETWORK };
    memcpy(event.data.network.bssid, info.bssid, sizeof info.bssid);
    memcpy(event.data.network.ssid, info.ssid, sizeof info.ssid);
    event.data.network.ssid_len = info.ssid_len;
    event.data.network.channel = s_cur_channel;
    event.data.network.rssi = pkt->rx_ctrl.rssi;
    event.data.network.has_ssid = info.has_ssid;
    event.data.network.privacy = (info.capability & (1u << 4)) != 0;
    event.data.network.has_rsn = has_rsn;
    event.data.network.mfp_capable = has_rsn && info.mfp_capable;
    event.data.network.mfp_required = has_rsn && info.mfp_required;
    event.data.network.interval_ms = (uint16_t)((info.interval_tu * 1024u + 500u) / 1000u);
    (void)ocp_event_submit(&event);
}

static void on_ble_adv(const uint8_t addr[6], const ble_adv_info_t *info,
                       int8_t rssi, void *ctx)
{
    (void)ctx;
    if (s_stop_req || !addr || !info) return;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool emit = !s_stop_req &&
                wardrive_cache_should_emit(&s_ble_cache, addr, rssi, now_ms());
    xSemaphoreGive(s_lock);
    if (!emit) return;

    ocp_event_record_t event = { .kind = OCP_EVENT_RECORD_BLE };
    memcpy(event.data.ble.mac, addr, 6);
    memcpy(event.data.ble.name, info->name, sizeof info->name);
    event.data.ble.name_len = info->name_len;
    event.data.ble.mfr_company_id = info->mfr_company_id;
    event.data.ble.has_name = info->has_name;
    event.data.ble.has_mfr = info->has_mfr;
    event.data.ble.is_tracker = info->is_tracker;
    event.data.ble.tracker = (uint8_t)info->tracker;
    event.data.ble.rssi = rssi;
    (void)ocp_event_submit(&event);
}

static void sleep_interruptible(uint32_t ms)
{
    uint32_t start = now_ms();
    while (!s_stop_req && (now_ms() - start) < ms) {
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

static void wardrive_task(void *arg)
{
    (void)arg;
    const wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
    size_t n_ch = 0;
    const uint8_t *chs = wifi_channels(&n_ch);

    while (!s_stop_req) {
        /* Phase 1: passive Wi-Fi sweep across active region/band/channel set. */
        if (n_ch > 0 &&
            esp_wifi_set_promiscuous_filter(&filter) == ESP_OK &&
            esp_wifi_set_promiscuous_rx_cb(on_wifi_frame) == ESP_OK &&
            esp_wifi_set_promiscuous(true) == ESP_OK) {
            for (size_t i = 0; i < n_ch && !s_stop_req; i++) {
                s_cur_channel = chs[i];
                if (esp_wifi_set_channel(chs[i], WIFI_SECOND_CHAN_NONE) != ESP_OK) {
                    ESP_LOGW(TAG, "hop to ch %u failed", chs[i]);
                    continue;
                }
                sleep_interruptible(s_wifi_dwell_ms);
            }
            (void)esp_wifi_set_promiscuous(false);
        } else {
            sleep_interruptible(100);
        }

        if (s_stop_req) break;

        /* Phase 2: duty-cycled passive BLE sweep (Wi-Fi promiscuous is off). */
        if (s_ble_enabled && ble_recon_ready()) {
            (void)ble_recon_wardrive_window(s_ble_dwell_ms, on_ble_adv, NULL, &s_stop_req);
        }
    }

    (void)esp_wifi_set_promiscuous(false);
    arbiter_release(PHY_OWNER_WARDRIVE);
    s_task_idle = true;
    vTaskDelete(NULL);
}

static void teardown(void)
{
    s_stop_req = true;
    for (uint32_t waited = 0; !s_task_idle && waited < WARDRIVE_STOP_WAIT_MS; waited += 5) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    (void)esp_wifi_set_promiscuous(false);
    arbiter_release(PHY_OWNER_WARDRIVE);
}

esp_err_t wifi_wardrive_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_wifi_buf = heap_caps_calloc(WARDRIVE_WIFI_CACHE_CAP, sizeof *s_wifi_buf,
                                  MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_wifi_buf) {
        s_wifi_buf = heap_caps_calloc(WARDRIVE_WIFI_CACHE_CAP, sizeof *s_wifi_buf,
                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    s_ble_buf = heap_caps_calloc(WARDRIVE_BLE_CACHE_CAP, sizeof *s_ble_buf,
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_ble_buf) {
        s_ble_buf = heap_caps_calloc(WARDRIVE_BLE_CACHE_CAP, sizeof *s_ble_buf,
                                     MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (!s_lock || !s_wifi_buf || !s_ble_buf) return ESP_ERR_NO_MEM;
    wardrive_cache_init(&s_wifi_cache, s_wifi_buf, WARDRIVE_WIFI_CACHE_CAP,
                        WARDRIVE_TTL_S_DEF * 1000u);
    wardrive_cache_init(&s_ble_cache, s_ble_buf, WARDRIVE_BLE_CACHE_CAP,
                        WARDRIVE_TTL_S_DEF * 1000u);
    return ESP_OK;
}

void wifi_cmd_start_wardrive(int argc, char **argv)
{
    uint32_t ble_flag = ble_recon_ready() ? 1u : 0u;
    uint32_t wifi_dwell = WARDRIVE_WIFI_DWELL_DEF;
    uint32_t ble_dwell = WARDRIVE_BLE_DWELL_DEF;
    uint32_t ttl_s = WARDRIVE_TTL_S_DEF;

    if (argc > 1) {
        if (!ocp_parse_u32(argv[1], &ble_flag) || ble_flag > 1u) {
            ocp_emit_error(OCP_ERR_BADARG, "ble must be 0 or 1");
            return;
        }
        if (ble_flag && !ble_recon_ready()) {
            ocp_emit_error(OCP_ERR_NOCAP, "ble unavailable");
            return;
        }
    }
    if (argc > 2) {
        if (!ocp_parse_u32(argv[2], &wifi_dwell) ||
            wifi_dwell < WARDRIVE_WIFI_DWELL_MIN || wifi_dwell > WARDRIVE_WIFI_DWELL_MAX) {
            ocp_emit_error(OCP_ERR_BADARG, "wifi_dwell_ms must be 50..1000");
            return;
        }
    }
    if (argc > 3) {
        if (!ocp_parse_u32(argv[3], &ble_dwell) ||
            ble_dwell < WARDRIVE_BLE_DWELL_MIN || ble_dwell > WARDRIVE_BLE_DWELL_MAX) {
            ocp_emit_error(OCP_ERR_BADARG, "ble_dwell_ms must be 250..5000");
            return;
        }
    }
    if (argc > 4) {
        if (!ocp_parse_u32(argv[4], &ttl_s) ||
            ttl_s < WARDRIVE_TTL_S_MIN || ttl_s > WARDRIVE_TTL_S_MAX) {
            ocp_emit_error(OCP_ERR_BADARG, "ttl_s must be 5..300");
            return;
        }
    }

    if (arbiter_acquire(PHY_OWNER_WARDRIVE, teardown) != ESP_OK) {
        char msg[48];
        snprintf(msg, sizeof msg, "radio in use by %s", arbiter_owner_name(arbiter_owner()));
        ocp_emit_error(OCP_ERR_BUSY, msg);
        return;
    }

    for (uint32_t waited = 0; !s_task_idle && waited < WARDRIVE_STOP_WAIT_MS; waited += 5) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    if (!s_task_idle) {
        arbiter_release(PHY_OWNER_WARDRIVE);
        ocp_emit_error(OCP_ERR_BUSY, "previous wardrive still stopping");
        return;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_ble_enabled = (ble_flag != 0);
    s_wifi_dwell_ms = wifi_dwell;
    s_ble_dwell_ms = ble_dwell;
    s_ttl_ms = ttl_s * 1000u;
    wardrive_cache_reset(&s_wifi_cache, s_ttl_ms);
    wardrive_cache_reset(&s_ble_cache, s_ttl_ms);
    s_stop_req = false;
    s_task_idle = false;
    xSemaphoreGive(s_lock);

    if (xTaskCreate(wardrive_task, "wardrive", 4096, NULL, 4, NULL) != pdPASS) {
        s_task_idle = true;
        arbiter_release(PHY_OWNER_WARDRIVE);
        ocp_emit_error(OCP_ERR_INTERNAL, "could not start wardrive task");
        return;
    }

    ocp_emit_compact(OCP_MARK_CFG, "%s=%s %s=%u %s=%lu %s=%lu %s=%lu",
                     OCP_K_MODE, OCP_OWNER_WARDRIVE,
                     OCP_K_BLE, s_ble_enabled ? 1u : 0u,
                     OCP_K_DWELL_MS, (unsigned long)s_wifi_dwell_ms,
                     OCP_K_BLE_MS, (unsigned long)s_ble_dwell_ms,
                     OCP_K_TTL_S, (unsigned long)ttl_s);
}
