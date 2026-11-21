/*
 * config.c — see config.h. Thin ESP-IDF NVS wrapper around config_codec.c.
 *
 * SPDX-License-Identifier: MIT
 */

#include "config.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"

#include "ocp.h"
#include "ocp_wifi_regions.h"
#include "wifi_channels.h"

static const char *TAG = "config";
static const char *NVS_NS = "oscilla";
static const char *NVS_KEY = "cfg_v1";

static probe_config_t s_cfg;
static bool s_loaded;

static bool write_blob(const probe_config_t *cfg)
{
    uint8_t blob[PROBE_CONFIG_BLOB_SIZE];
    if (!probe_config_encode(cfg, blob)) return false;

    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open: %s", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_blob(h, NVS_KEY, blob, sizeof blob);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_commit: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

void probe_config_load(void)
{
    probe_config_defaults(&s_cfg);
    s_loaded = true;

    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err == ESP_OK) {
        uint8_t blob[PROBE_CONFIG_BLOB_SIZE];
        size_t len = sizeof blob;
        err = nvs_get_blob(h, NVS_KEY, blob, &len);
        nvs_close(h);
        if (err == ESP_OK) {
            if (!probe_config_decode(blob, len, &s_cfg)) {
                ESP_LOGW(TAG, "corrupt/invalid NVS config; using defaults");
                probe_config_defaults(&s_cfg);
            }
        } else if (err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "nvs_get_blob: %s", esp_err_to_name(err));
        }
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "nvs_open(ro): %s", esp_err_to_name(err));
    }

    const ocp_wifi_region_t *r = ocp_wifi_region_find(s_cfg.wifi_region);
    if (r) wifi_region_select(r);
    wifi_band_select((wifi_band_scope_t)s_cfg.wifi_band);
    if (s_cfg.wifi_channels[0] != '\0' && strcmp(s_cfg.wifi_channels, OCP_CHANNELS_ALL) != 0) {
        (void)wifi_channels_set(s_cfg.wifi_channels);
    }

    ESP_LOGI(TAG, "loaded region=%s band=%s custom_ch=%d lora=%d",
             wifi_region()->code, wifi_band_scope_name(wifi_band_scope()),
             wifi_channels_is_custom() ? 1 : 0, (int)s_cfg.lora_valid);
}

const probe_config_t *probe_config_get(void)
{
    if (!s_loaded) probe_config_defaults(&s_cfg);
    return &s_cfg;
}

bool probe_config_save_wifi(void)
{
    if (!s_loaded) probe_config_defaults(&s_cfg);

    probe_config_t next = s_cfg;
    memset(next.wifi_region, 0, sizeof next.wifi_region);
    strncpy(next.wifi_region, wifi_region()->code, sizeof next.wifi_region - 1);
    next.wifi_band = (uint8_t)wifi_band_scope();
    memset(next.wifi_channels, 0, sizeof next.wifi_channels);
    if (wifi_channels_is_custom()) {
        wifi_channels_format_list(next.wifi_channels, sizeof next.wifi_channels);
    }

    if (!write_blob(&next)) return false;
    s_cfg = next;
    return true;
}

bool probe_config_save_lora(uint32_t freq_hz, uint8_t sf, uint16_t bw_khz,
                            uint8_t cr, uint8_t sync_word)
{
    if (!s_loaded) probe_config_defaults(&s_cfg);

    probe_config_t next = s_cfg;
    next.lora_valid = 1;
    next.lora_freq_hz = freq_hz;
    next.lora_sf = sf;
    next.lora_bw_khz = bw_khz;
    next.lora_cr = cr;
    next.lora_sync_word = sync_word;

    if (!write_blob(&next)) return false;
    s_cfg = next;
    return true;
}
