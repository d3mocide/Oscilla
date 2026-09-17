/* zig_radio.c — ESP-IDF callbacks are ISR-context; keep this handoff tiny. */
#include "zig_radio.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_attr.h"
#include "esp_ieee802154.h"
#include "esp_timer.h"

static QueueHandle_t s_rxq;
static volatile bool s_active;
static volatile uint16_t s_dropped;

esp_err_t zig_radio_init(void)
{
    s_rxq = xQueueCreate(12, sizeof(zig_rx_t));
    return s_rxq ? ESP_OK : ESP_ERR_NO_MEM;
}

void IRAM_ATTR esp_ieee802154_receive_done(uint8_t *frame, esp_ieee802154_frame_info_t *info)
{
    zig_rx_t item = {0};
    uint8_t len = frame ? frame[0] : 0;
    if (s_active && len >= 3 && len <= 127) {
        item.len = (uint8_t)(len + 1); /* frame includes PHY length byte */
        memcpy(item.data, frame, item.len);
        item.ch = info->channel; item.rssi = info->rssi; item.lqi = info->lqi;
        item.ts_ms = (uint32_t)(info->timestamp / 1000);
        BaseType_t woke = pdFALSE;
        if (xQueueSendFromISR(s_rxq, &item, &woke) != pdTRUE) ++s_dropped;
        if (woke) portYIELD_FROM_ISR();
    }
    /* The vendor buffer is never retained past this ISR, including overflow. */
    if (frame) esp_ieee802154_receive_handle_done(frame);
}

esp_err_t zig_radio_start(uint8_t channel)
{
    if (channel < 11 || channel > 26) return ESP_ERR_INVALID_ARG;
    xQueueReset(s_rxq); s_dropped = 0;
    esp_err_t err = esp_ieee802154_enable();
#ifdef CONFIG_OSCILLA_ZIG_ACK_TEST_IDENTITY
    if (err == ESP_OK) err = esp_ieee802154_set_panid(CONFIG_OSCILLA_ZIG_ACK_TEST_PANID);
    if (err == ESP_OK) err = esp_ieee802154_set_short_address(CONFIG_OSCILLA_ZIG_ACK_TEST_SHORT_ADDR);
#endif
    if (err == ESP_OK) err = esp_ieee802154_set_promiscuous(true); /* disables auto-ACK TX in ESP-IDF */
    if (err == ESP_OK) err = esp_ieee802154_set_channel(channel);
    if (err == ESP_OK) { s_active = true; err = esp_ieee802154_receive(); }
    if (err != ESP_OK) { s_active = false; esp_ieee802154_disable(); }
    return err;
}

void zig_radio_stop(void)
{
    s_active = false;
    /* Do not set promiscuous=false while enabled: ESP-IDF would re-enable auto ACK. */
    esp_ieee802154_disable();
    xQueueReset(s_rxq);
}

void zig_radio_rearm(void)
{
    if (s_active && esp_ieee802154_get_state() != ESP_IEEE802154_RADIO_RECEIVE) (void)esp_ieee802154_receive();
}

bool zig_radio_next(zig_rx_t *out, uint32_t wait_ms)
{
    return xQueueReceive(s_rxq, out, pdMS_TO_TICKS(wait_ms)) == pdTRUE;
}

esp_err_t zig_radio_set_channel(uint8_t channel)
{
    return (channel >= 11 && channel <= 26) ? esp_ieee802154_set_channel(channel) : ESP_ERR_INVALID_ARG;
}

uint16_t zig_radio_dropped(void) { return s_dropped; }
