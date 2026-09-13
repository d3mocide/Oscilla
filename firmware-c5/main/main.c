/*
 * main.c — oscilla-c5 (the probe).
 *
 * Boot order (DESIGN §6.1): NVS -> arbiter -> platform -> OCP server.
 * LoRa lands in P3.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_frame.h"
#include "ocp_server.h"
#include "ocp_transport.h"
#include "radio_arbiter.h"
#include "status_led.h"
#include "wifi_inspect.h"
#include "wifi_recon.h"

#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "oscilla";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(status_led_start());    /* first, so boot is visible */
    ESP_ERROR_CHECK(arbiter_init());

    /* A radio that fails to come up stays local: the probe still answers, it
     * just doesn't advertise that cap. */
    err = wifi_recon_init();
    if (err == ESP_OK) err = wifi_inspect_init();
    if (err != ESP_OK) ESP_LOGE(TAG, "wifi unavailable: %s", esp_err_to_name(err));

    ESP_ERROR_CHECK(ocp_transport_init());
    ESP_ERROR_CHECK(ocp_frame_init());
    ESP_ERROR_CHECK(ocp_server_start());

    ESP_LOGI(TAG, "probe up on %s", ocp_transport_name());
}
