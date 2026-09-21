/*
 * zig_emitter.c — see ../README.md. NOT Oscilla: this is the one place in
 * this workspace that transmits on purpose, on a second, separate C5, so
 * Oscilla's own receive-only Mesh engine has real 802.15.4 frames to see
 * on the bench.
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_ieee802154.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "zig_emit";

/* Fixed test identity — obviously synthetic, never a real network's PAN. */
#define TEST_PAN   0xBEEF
#define TEST_SRC   0x1234
#define TEST_DST   0xFFFF /* 802.15.4 broadcast short address */

#define CH_MIN 11
#define CH_MAX 26
#define TX_PERIOD_MS 150

/* Same XIAO ESP32-C5 user LED Oscilla's own status_led.c uses (Rev D
 * §8.1): GPIO27, active-low, a strapping pin so only ever driven after
 * boot (i.e. from inside app_main, never earlier).
 *   fast blink   pre-radio heartbeat (console/boot alive)
 *   solid on     nvs or radio init failed
 *   slow blink   in the main transmit loop */
#define LED_GPIO 27
static void led(bool on) { gpio_set_level(LED_GPIO, on ? 0 : 1); }
static void led_init(void)
{
    const gpio_config_t cfg = { .pin_bit_mask = 1ULL << LED_GPIO, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&cfg);
    led(false);
}

/* Minimal standards-shaped MHR: 16-bit dst PAN+addr, PAN-ID compression,
 * 16-bit src addr, no payload. `esp_ieee802154_transmit()` wants
 * [Len][MHR][Payload] and computes+appends the FCS itself (see the API
 * doc comment) — Len therefore counts MHR+Payload+2 (FCS), not just what
 * is actually written into this buffer. */
static size_t build_frame(uint8_t *buf, uint8_t seq)
{
    /* FCF: type=1 (data), bit6 PAN-ID compression, dst_mode=2 (short),
     * src_mode=2 (short) — see zig_frame.c for the receive-side decode
     * this must satisfy. */
    const uint16_t fcf = 0x1u | (1u << 6) | (2u << 10) | (2u << 14);

    uint8_t *mhr = buf + 1;
    mhr[0] = (uint8_t)(fcf & 0xFF);
    mhr[1] = (uint8_t)(fcf >> 8);
    mhr[2] = seq;
    mhr[3] = (uint8_t)(TEST_PAN & 0xFF);
    mhr[4] = (uint8_t)(TEST_PAN >> 8);
    mhr[5] = (uint8_t)(TEST_DST & 0xFF);
    mhr[6] = (uint8_t)(TEST_DST >> 8);
    mhr[7] = (uint8_t)(TEST_SRC & 0xFF);
    mhr[8] = (uint8_t)(TEST_SRC >> 8);

    const uint8_t mhr_len = 9;
    buf[0] = (uint8_t)(mhr_len + 2); /* +FCS, hardware-appended */
    return 1 + mhr_len;
}

/* Solid on, log, and halt here — never abort/reboot on a fault, so the LED
 * state stays meaningful instead of flipping into a reboot loop. */
static void fault_halt(const char *what, esp_err_t err)
{
    ESP_LOGE(TAG, "FAULT: %s: %s", what, esp_err_to_name(err));
    led(true);
    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main(void)
{
    led_init();

    /* Staged, unconditional heartbeat before touching the radio at all —
     * proves the console (and LED) work, independent of anything below.
     * Fast blink = this phase. */
    for (int i = 0; i < 6; i++) {
        ESP_LOGI(TAG, "alive (pre-radio) %d/6", i + 1);
        led(i % 2 == 0);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    led(false);

    ESP_LOGI(TAG, "nvs_flash_init...");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err != ESP_OK) fault_halt("nvs_flash_erase", err);
        err = nvs_flash_init();
    }
    if (err != ESP_OK) fault_halt("nvs_flash_init", err);
    ESP_LOGI(TAG, "nvs_flash_init done");

    ESP_LOGI(TAG, "esp_ieee802154_enable...");
    err = esp_ieee802154_enable();
    if (err != ESP_OK) fault_halt("esp_ieee802154_enable", err);
    ESP_LOGI(TAG, "esp_ieee802154_enable done");

    ESP_LOGW(TAG, "TRANSMITTING test 802.15.4 frames — pan=%04x src=%04x, "
                  "ch %d..%d, one every %dms. Bench-supervised use only.",
             TEST_PAN, TEST_SRC, CH_MIN, CH_MAX, TX_PERIOD_MS);

    uint8_t ch = CH_MIN;
    uint8_t seq = 0;
    int blink_phase = 0;
    for (;;) {
        esp_err_t chan_err = esp_ieee802154_set_channel(ch);
        if (chan_err != ESP_OK) {
            ESP_LOGW(TAG, "set_channel(%u): %s", ch, esp_err_to_name(chan_err));
        } else {
            uint8_t frame[16];
            size_t len = build_frame(frame, seq++);
            esp_err_t tx_err = esp_ieee802154_transmit(frame, /*cca=*/false);
            if (tx_err != ESP_OK) {
                ESP_LOGW(TAG, "transmit ch=%u: %s", ch, esp_err_to_name(tx_err));
            } else {
                ESP_LOGI(TAG, "tx ch=%u seq=%u (%u bytes queued)", ch, seq - 1, (unsigned)len);
            }
        }
        /* Slow blink (~1s period) in steady state — visibly distinct from
         * the fast pre-radio blink above. */
        blink_phase = (blink_phase + 1) % ((1000 / TX_PERIOD_MS) | 1);
        led(blink_phase == 0);
        ch = (ch == CH_MAX) ? CH_MIN : (uint8_t)(ch + 1);
        vTaskDelay(pdMS_TO_TICKS(TX_PERIOD_MS));
    }
}
