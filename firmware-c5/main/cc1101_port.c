/*
 * cc1101_port.c — see cc1101_port.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_port.h"

#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "subghz_spi.h"

#define PIN_CC1101_CSN 7          /* D3, 10 kΩ pull-up (Rev D §4.4) */
#define CC1101_SPI_HZ  1000000    /* Rev D §4: start at 1 MHz */

typedef struct {
    spi_device_handle_t dev;
    bool locked;
} port_t;

static port_t s_port;

static void port_cs(void *ctx, bool asserted)
{
    port_t *p = ctx;
    if (asserted) {
        if (!p->locked) p->locked = subghz_spi_lock(SUBGHZ_SPI_LOCK_TIMEOUT_MS);
        if (p->locked) gpio_set_level(PIN_CC1101_CSN, 0);
    } else {
        gpio_set_level(PIN_CC1101_CSN, 1);
        if (p->locked) {
            p->locked = false;
            subghz_spi_unlock();
        }
    }
}

/* Without the bus lock the transfer is refused, so a timed-out lock never
 * clocks the bus while the Wio might be selected. */
static bool port_xfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len)
{
    port_t *p = ctx;
    if (!p->locked || len == 0 || len > 1 + CC1101_FIFO_SIZE) return false;
    spi_transaction_t t = { .length = len * 8, .tx_buffer = tx, .rx_buffer = rx };
    return spi_device_polling_transmit(p->dev, &t) == ESP_OK;
}

/* SO read as a GPIO for CHIP_RDYn; low only means "ready" while selected. */
static int port_so(void *ctx)
{
    const port_t *p = ctx;
    return p->locked ? gpio_get_level(SUBGHZ_PIN_MISO) : 1;
}

static void port_delay(void *ctx, uint32_t us)
{
    (void)ctx;
    esp_rom_delay_us(us);
}

static uint32_t port_now(void *ctx)
{
    (void)ctx;
    return (uint32_t)esp_timer_get_time();
}

esp_err_t cc1101_port_init(cc1101_bus_t *bus)
{
    if (!bus) return ESP_ERR_INVALID_ARG;

    const gpio_config_t cs_cfg = { .pin_bit_mask = 1ULL << PIN_CC1101_CSN, .mode = GPIO_MODE_OUTPUT };
    esp_err_t err = gpio_config(&cs_cfg);
    if (err != ESP_OK) return err;
    gpio_set_level(PIN_CC1101_CSN, 1);

    err = subghz_spi_bus_init(1 + CC1101_FIFO_SIZE + 8);
    if (err != ESP_OK) return err;
    const spi_device_interface_config_t dev_cfg = {
        .mode = 0,                 /* CPOL=0, CPHA=0 (SWRS061I §10) */
        .clock_speed_hz = CC1101_SPI_HZ,
        .spics_io_num = -1,        /* CSn driven by port_cs under the bus lock */
        .queue_size = 1,
    };
    err = spi_bus_add_device(SUBGHZ_SPI_HOST, &dev_cfg, &s_port.dev);
    if (err != ESP_OK) return err;

    *bus = (cc1101_bus_t){ &s_port, port_xfer, port_cs, port_so, port_delay, port_now };
    return ESP_OK;
}
