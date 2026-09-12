/*
 * ocp_transport.c — see ocp_transport.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_transport.h"

#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#define OCP_RX_BUF_BYTES 1024
#define OCP_TX_BUF_BYTES 1024

#if CONFIG_OSCILLA_OCP_TRANSPORT_UART

#include "driver/uart.h"
#include "ocp.h"

/* Rev D §3: C5 GPIO11 TX (XIAO D6) -> deck RX; GPIO12 RX (D7) <- deck TX. */
#define OCP_UART_PORT   UART_NUM_0
#define OCP_UART_TX_PIN 11
#define OCP_UART_RX_PIN 12

esp_err_t ocp_transport_init(void)
{
    const uart_config_t cfg = {
        .baud_rate = OCP_BAUD_DEFAULT,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(OCP_UART_PORT, OCP_RX_BUF_BYTES,
                                        OCP_TX_BUF_BYTES, 0, NULL, 0);
    if (err != ESP_OK) return err;

    err = uart_param_config(OCP_UART_PORT, &cfg);
    if (err != ESP_OK) return err;

    return uart_set_pin(OCP_UART_PORT, OCP_UART_TX_PIN, OCP_UART_RX_PIN,
                        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int ocp_transport_read(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    int n = uart_read_bytes(OCP_UART_PORT, buf, len, pdMS_TO_TICKS(timeout_ms));
    return n < 0 ? 0 : n;
}

void ocp_transport_write(const char *data, size_t len)
{
    uart_write_bytes(OCP_UART_PORT, data, len);
}

const char *ocp_transport_name(void) { return "uart0"; }

#else /* CONFIG_OSCILLA_OCP_TRANSPORT_USB */

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"

esp_err_t ocp_transport_init(void)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.rx_buffer_size = OCP_RX_BUF_BYTES;
    cfg.tx_buffer_size = OCP_TX_BUF_BYTES;
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err != ESP_OK) return err;

    /* Route console writes through the same driver, so log lines serialise
     * with frames instead of interleaving mid-line. */
    usb_serial_jtag_vfs_use_driver();
    return ESP_OK;
}

int ocp_transport_read(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    int n = usb_serial_jtag_read_bytes(buf, len, pdMS_TO_TICKS(timeout_ms));
    return n < 0 ? 0 : n;
}

void ocp_transport_write(const char *data, size_t len)
{
    /* Bounded wait: a host that stops reading must not wedge the probe. */
    usb_serial_jtag_write_bytes(data, len, pdMS_TO_TICKS(100));
}

const char *ocp_transport_name(void) { return "usb"; }

#endif
