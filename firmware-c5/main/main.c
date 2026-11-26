/*
 * main.c — oscilla-c5 (the probe).
 *
 * Boot order (DESIGN §6.1): NVS -> arbiters -> platform -> OCP server.
 * LoRa lands in P3 through the Wio-SX1262 harness.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ble_recon.h"
#include "cc1101_bench.h"
#include "synth_bench.h"
#include "ook_recon.h"
#include "config.h"
#include "lora_recon.h"
#include "ocp_frame.h"
#include "ocp_server.h"
#include "ocp_transport.h"
#include "radio_arbiter.h"
#include "status_led.h"
#include "wifi_deauth.h"
#include "wifi_hunt.h"
#include "wifi_inspect.h"
#include "wifi_networks.h"
#include "wifi_recon.h"
#include "wifi_sniff.h"
#include "wifi_spectrum.h"
#include "wifi_wardrive.h"
#include "zig_recon.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

static const char *TAG = "oscilla";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    probe_config_load();

    ESP_ERROR_CHECK(status_led_start());    /* first, so boot is visible */
    ESP_ERROR_CHECK(arbiter_init());

    /* A radio that fails to come up stays local: the probe still answers, it
     * just doesn't advertise that cap. */
    err = wifi_recon_init();
    if (err == ESP_OK) err = wifi_inspect_init();
    if (err == ESP_OK) err = wifi_networks_init();
    if (err == ESP_OK) err = wifi_sniff_init();
    if (err == ESP_OK) err = wifi_deauth_init();
    if (err == ESP_OK) err = wifi_spectrum_init();
    if (err == ESP_OK) err = wifi_wardrive_init();
    if (err == ESP_OK) err = wifi_hunt_init();
    if (err != ESP_OK) ESP_LOGE(TAG, "wifi unavailable: %s", esp_err_to_name(err));

    if (lora_recon_init() != ESP_OK) {
        ESP_LOGE(TAG, "lora unavailable");
    } else {
        const probe_config_t *pcfg = probe_config_get();
        if (pcfg->lora_valid) {
            (void)lora_recon_apply_saved(pcfg->lora_freq_hz, pcfg->lora_sf,
                                         pcfg->lora_bw_khz, pcfg->lora_cr,
                                         pcfg->lora_sync_word);
        }
    }

#if CONFIG_OSCILLA_CC1101_BENCH
    /* The bench console owns the CC1101; the OOK lane stays absent. */
    if (cc1101_bench_start() != ESP_OK) ESP_LOGE(TAG, "cc1101 bench unavailable");
#else
    /* After LoRa: the CC1101 shares its SPI bus. A missing module is fine. */
    if (ook_recon_init() != ESP_OK) ESP_LOGE(TAG, "ook unavailable");
#endif
#if CONFIG_OSCILLA_SYNTH_BENCH
    if (synth_bench_start() != ESP_OK) ESP_LOGE(TAG, "synth bench unavailable");
#endif

    /* BLE is async (ble_recon_ready() flips true once the NimBLE host
     * actually syncs, not merely when this call returns) — cap_available()
     * checking OCP_CAP_BLE via ble_recon_ready() covers the narrow boot
     * window before that, same "stays local" posture as the others. */
    if (ble_recon_init() != ESP_OK) ESP_LOGE(TAG, "ble unavailable");
    if (zig_recon_init() != ESP_OK) ESP_LOGE(TAG, "802154 unavailable");

    ESP_ERROR_CHECK(ocp_transport_init());
    ESP_ERROR_CHECK(ocp_frame_init());
    ESP_ERROR_CHECK(ocp_server_start());

    ESP_LOGI(TAG, "probe up on %s", ocp_transport_name());
}
