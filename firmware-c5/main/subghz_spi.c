/*
 * subghz_spi.c — see subghz_spi.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "subghz_spi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_bus_lock;
static StaticSemaphore_t s_bus_lock_mem;
static bool s_bus_up;

esp_err_t subghz_spi_bus_init(size_t max_transfer)
{
    if (!s_bus_lock) s_bus_lock = xSemaphoreCreateMutexStatic(&s_bus_lock_mem);
    if (s_bus_up) return ESP_OK;

    const spi_bus_config_t bus_cfg = {
        .sclk_io_num = SUBGHZ_PIN_SCK,
        .mosi_io_num = SUBGHZ_PIN_MOSI,
        .miso_io_num = SUBGHZ_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = (int)max_transfer,
    };
    const esp_err_t err = spi_bus_initialize(SUBGHZ_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    s_bus_up = true;
    return ESP_OK;
}

bool subghz_spi_lock(uint32_t timeout_ms)
{
    return s_bus_lock && xSemaphoreTake(s_bus_lock, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void subghz_spi_unlock(void)
{
    if (s_bus_lock) xSemaphoreGive(s_bus_lock);
}
