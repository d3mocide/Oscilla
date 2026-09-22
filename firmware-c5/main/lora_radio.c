/*
 * lora_radio.c — see lora_radio.h.
 *
 * SPI transaction shapes are transcribed from the official SX1261/2
 * datasheet (Semtech, Rev 1.2, June 2019), not guessed or lifted from a
 * generic library — Rev D warns third-party SX126x drivers tend to assume a
 * bare-SX1262 board and mishandle the Wio's RF-switch/TCXO wiring (D-11).
 *
 * Locking: s_lock guards s_running and the SPI bus against concurrent
 * rx_start/rx_stop/is_running calls from the dispatch task while the radio
 * task is mid-transaction. The radio task itself is the only reader of
 * DIO1 events; nothing else touches the bus once RX is running.
 *
 * No SPI inside the DIO1 ISR (AGENTS.md gotcha 8's rule, applied to this
 * radio too): the ISR only posts to s_dio1_queue. All bus work — including
 * every bounded BUSY wait — happens in lora_task.
 *
 * SPDX-License-Identifier: MIT
 */

#include "lora_radio.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "lora_radio";

/* Rev D pin map, custom harness (AGENTS.md gotcha 16). */
#define PIN_SCK    8
#define PIN_MISO   9
#define PIN_MOSI   10
#define PIN_NSS    23
#define PIN_RST    1
#define PIN_DIO1   0
#define PIN_BUSY   24
#define PIN_RF_SW  25

#define SPI_HOST_ID       SPI2_HOST
#define SPI_INITIAL_HZ    1000000     /* Rev D §4: start ~1 MHz */
#define BUSY_TIMEOUT_MS   1000        /* covers the 3.5 ms full calibration with generous margin */

/* Opcodes, SX1261/2 datasheet §11 command tables. */
#define OP_SET_SLEEP              0x84
#define OP_SET_STANDBY            0x80
#define OP_SET_RX                 0x82
#define OP_SET_REGULATOR_MODE     0x96
#define OP_CALIBRATE              0x89
#define OP_SET_DIO3_TCXO_CTRL     0x97
#define OP_SET_DIO2_RF_SWITCH     0x9D
#define OP_SET_DIO_IRQ_PARAMS     0x08
#define OP_GET_IRQ_STATUS         0x12
#define OP_CLEAR_IRQ_STATUS       0x02
#define OP_SET_RF_FREQUENCY       0x86
#define OP_SET_PACKET_TYPE        0x8A
#define OP_SET_MODULATION_PARAMS  0x8B
#define OP_SET_PACKET_PARAMS      0x8C
#define OP_SET_BUFFER_BASE_ADDR   0x8F
#define OP_GET_RX_BUFFER_STATUS   0x13
#define OP_GET_PACKET_STATUS      0x14
#define OP_READ_BUFFER            0x1E
#define OP_GET_STATUS             0xC0
#define OP_GET_DEVICE_ERRORS      0x17
#define OP_CLEAR_DEVICE_ERRORS    0x07

#define STDBY_RC    0x00
#define STDBY_XOSC  0x01

#define PACKET_TYPE_LORA  0x01

/* IRQ bits, Table 13-29. */
#define IRQ_RX_DONE     (1u << 1)
#define IRQ_HEADER_ERR  (1u << 5)
#define IRQ_CRC_ERR     (1u << 6)
#define IRQ_TIMEOUT     (1u << 9)
#define IRQ_RX_MASK     (IRQ_RX_DONE | IRQ_HEADER_ERR | IRQ_CRC_ERR | IRQ_TIMEOUT)

/* OpError bits, Table 13-85. */
#define OPERR_XOSC_START_ERR  (1u << 5)

#define FXTAL_HZ  32000000ULL   /* Wio-SX1262 module datasheet §2: 32 MHz TCXO */

/* D-10: voltage settled, delay is a bench-verified starting point. */
#define TCXO_VOLTAGE_1V8  0x02
#define TCXO_DELAY_10MS   640    /* 640 * 15.625us = 10ms */

static spi_device_handle_t s_spi;
static SemaphoreHandle_t s_lock;
static QueueHandle_t s_dio1_queue;   /* ISR -> radio task, holds nothing but a tick tag */
static QueueHandle_t s_event_queue;  /* radio task -> caller (lora_recon.c) */
static TaskHandle_t s_task;
static volatile bool s_running;
static volatile lora_radio_stats_t s_stats;

static bool rx_write_opcode_allowed(uint8_t opcode)
{
    switch (opcode) {
        case OP_SET_SLEEP:
        case OP_SET_STANDBY:
        case OP_SET_RX:
        case OP_SET_REGULATOR_MODE:
        case OP_CALIBRATE:
        case OP_SET_DIO3_TCXO_CTRL:
        case OP_SET_DIO2_RF_SWITCH:
        case OP_SET_DIO_IRQ_PARAMS:
        case OP_CLEAR_IRQ_STATUS:
        case OP_SET_RF_FREQUENCY:
        case OP_SET_PACKET_TYPE:
        case OP_SET_MODULATION_PARAMS:
        case OP_SET_PACKET_PARAMS:
        case OP_SET_BUFFER_BASE_ADDR:
        case OP_CLEAR_DEVICE_ERRORS:
            return true;
        default:
            return false;
    }
}

static bool rx_read_opcode_allowed(uint8_t opcode)
{
    switch (opcode) {
        case OP_GET_IRQ_STATUS:
        case OP_GET_RX_BUFFER_STATUS:
        case OP_GET_PACKET_STATUS:
        case OP_GET_STATUS:
        case OP_GET_DEVICE_ERRORS:
            return true;
        default:
            return false;
    }
}

/* ---- low-level bus helpers --------------------------------------------- */

static void cs_select(void)   { gpio_set_level(PIN_NSS, 0); }
static void cs_deselect(void) { gpio_set_level(PIN_NSS, 1); }

/* Bounded: BUSY must fall before any new command, or fault rather than hang.
 * See Rev D §4.3. */
static esp_err_t wait_busy_low(void)
{
    int64_t deadline = esp_timer_get_time() + (int64_t)BUSY_TIMEOUT_MS * 1000;
    while (gpio_get_level(PIN_BUSY)) {
        if (esp_timer_get_time() > deadline) {
            ESP_LOGE(TAG, "BUSY stuck high past %d ms", BUSY_TIMEOUT_MS);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_OK;
}

/* opcode + up to 9 payload bytes, no response expected beyond the status
 * byte every transaction returns (which we discard here). */
static esp_err_t cmd_write(uint8_t opcode, const uint8_t *data, size_t len)
{
    if (!rx_write_opcode_allowed(opcode)) return ESP_ERR_NOT_SUPPORTED;
    esp_err_t err = wait_busy_low();
    if (err != ESP_OK) return err;

    uint8_t tx[10] = { opcode };
    if (len > sizeof(tx) - 1) return ESP_ERR_INVALID_SIZE;
    if (len) memcpy(&tx[1], data, len);

    spi_transaction_t t = { .length = (len + 1) * 8, .tx_buffer = tx };
    cs_select();
    err = spi_device_transmit(s_spi, &t);
    cs_deselect();
    return err;
}

/* opcode + NOP*len, returns the `len` bytes after the RFU+Status header
 * (GetIrqStatus/GetRxBufferStatus/GetPacketStatus/GetStatus all share this
 * shape: byte 0 = RFU, byte 1 = Status, payload from byte 2). */
static esp_err_t cmd_read(uint8_t opcode, uint8_t *out, size_t len)
{
    if (!rx_read_opcode_allowed(opcode)) return ESP_ERR_NOT_SUPPORTED;
    esp_err_t err = wait_busy_low();
    if (err != ESP_OK) return err;

    uint8_t tx[10] = { opcode };
    uint8_t rx[10] = { 0 };
    if (len + 1 > sizeof(tx) - 1) return ESP_ERR_INVALID_SIZE;

    spi_transaction_t t = {
        .length = (len + 2) * 8,   /* opcode + RFU + Status + len payload bytes, minus opcode itself on tx */
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    cs_select();
    err = spi_device_transmit(s_spi, &t);
    cs_deselect();
    if (err == ESP_OK && out) memcpy(out, &rx[2], len);
    return err;
}

/* ReadBuffer's own shape (§13.2.4): opcode, offset, NOP, then data. */
static esp_err_t read_buffer(uint8_t offset, uint8_t *out, size_t len)
{
    esp_err_t err = wait_busy_low();
    if (err != ESP_OK) return err;

    uint8_t tx[3 + LORA_MAX_PAYLOAD] = { OP_READ_BUFFER, offset, 0x00 };
    uint8_t rx[3 + LORA_MAX_PAYLOAD] = { 0 };
    if (len > LORA_MAX_PAYLOAD) return ESP_ERR_INVALID_SIZE;

    spi_transaction_t t = { .length = (3 + len) * 8, .tx_buffer = tx, .rx_buffer = rx };
    cs_select();
    err = spi_device_transmit(s_spi, &t);
    cs_deselect();
    if (err == ESP_OK) memcpy(out, &rx[3], len);
    return err;
}

/* ---- RF-switch coherence (Rev D §4.4) ---------------------------------- */

/* External GPIO25 and the chip's internal DIO2 must both say "receive" —
 * enabling DIO2 alone does not manage the external GPIO. High = receive,
 * per Rev D's table. */
static void rf_switch_set_receive(bool on)
{
    gpio_set_level(PIN_RF_SW, on ? 1 : 0);
}

/* ---- init / reset ------------------------------------------------------- */

static void dio1_isr(void *arg);   /* IRAM_ATTR on the definition only — a second copy on the
                                     * prototype makes GCC allocate two distinct iram sections. */
static void lora_task(void *arg);

esp_err_t lora_radio_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;

    s_dio1_queue = xQueueCreate(8, sizeof(uint8_t));
    s_event_queue = xQueueCreate(8, sizeof(lora_event_t));
    if (!s_dio1_queue || !s_event_queue) return ESP_ERR_NO_MEM;

    const gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_NSS) | (1ULL << PIN_RST) | (1ULL << PIN_RF_SW),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&out_cfg);
    if (err != ESP_OK) return err;
    cs_deselect();
    gpio_set_level(PIN_RST, 1);      /* released; active low */
    rf_switch_set_receive(false);

    const gpio_config_t busy_cfg = { .pin_bit_mask = 1ULL << PIN_BUSY, .mode = GPIO_MODE_INPUT };
    err = gpio_config(&busy_cfg);
    if (err != ESP_OK) return err;

    const gpio_config_t dio1_cfg = {
        .pin_bit_mask = 1ULL << PIN_DIO1,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    err = gpio_config(&dio1_cfg);
    if (err != ESP_OK) return err;

    const spi_bus_config_t bus_cfg = {
        .sclk_io_num = PIN_SCK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LORA_MAX_PAYLOAD + 8,
    };
    err = spi_bus_initialize(SPI_HOST_ID, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return err;

    const spi_device_interface_config_t dev_cfg = {
        .mode = 0,   /* CPOL=0, CPHA=0 per datasheet §8.2 */
        .clock_speed_hz = SPI_INITIAL_HZ,
        .spics_io_num = -1,   /* NSS driven manually: BUSY must be checked before each select */
        .queue_size = 1,
    };
    err = spi_bus_add_device(SPI_HOST_ID, &dev_cfg, &s_spi);
    if (err != ESP_OK) return err;

    /* ISR only posts to s_dio1_queue (AGENTS.md gotcha 8's rule) — all SPI
     * work happens in lora_task, which drains the queue it feeds. */
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;   /* ALREADY_EXISTS-equivalent: fine, shared service */
    err = gpio_isr_handler_add(PIN_DIO1, dio1_isr, NULL);
    if (err != ESP_OK) return err;

    return xTaskCreate(lora_task, "lora_radio", 4096, NULL, 4, &s_task) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

/* NRESET held low >=100us per datasheet §8.1, then the chip auto-calibrates
 * and BUSY stays high until that completes — wait_busy_low bounds it. */
static esp_err_t hw_reset(void)
{
    gpio_set_level(PIN_RST, 0);
    esp_rom_delay_us(150);
    gpio_set_level(PIN_RST, 1);
    return wait_busy_low();
}

/* ---- modem bring-up ------------------------------------------------------ */

static esp_err_t lora_bw_cr_sf_to_modparams(const lora_rx_params_t *p)
{
    /* LDRO per datasheet §13.4.5: usually needed when symbol time >=16.38ms
     * (SF11 @ BW125, SF12 @ BW125/BW250). */
    uint8_t ldro = ((p->bw == LORA_BW_125 && p->sf >= 11) ||
                    (p->bw == LORA_BW_250 && p->sf == 12)) ? 0x01 : 0x00;
    uint8_t data[4] = { p->sf, (uint8_t)p->bw, p->cr, ldro };
    return cmd_write(OP_SET_MODULATION_PARAMS, data, sizeof data);
}

static esp_err_t lora_set_rf_frequency(uint32_t freq_hz)
{
    /* RF frequency = RfFreq * Fxtal / 2^25 (datasheet §13.4.1). */
    uint32_t rf_freq = (uint32_t)(((uint64_t)freq_hz << 25) / FXTAL_HZ);
    uint8_t data[4] = {
        (uint8_t)(rf_freq >> 24), (uint8_t)(rf_freq >> 16),
        (uint8_t)(rf_freq >> 8),  (uint8_t)rf_freq,
    };
    return cmd_write(OP_SET_RF_FREQUENCY, data, sizeof data);
}

static esp_err_t lora_set_packet_params(void)
{
    /* Explicit header (payload length comes from the header itself, since
     * we don't know incoming packet length ahead of time), CRC on, standard
     * IQ. 8-symbol preamble is a conservative default, not a spec-mandated
     * one — first thing to tune once real packets are seen on the bench. */
    uint8_t data[6] = {
        0x00, 0x08,   /* preamble length = 8 symbols */
        0x00,         /* explicit header */
        LORA_MAX_PAYLOAD,
        0x01,         /* CRC on */
        0x00,         /* standard IQ */
    };
    return cmd_write(OP_SET_PACKET_PARAMS, data, sizeof data);
}

/* Full RX bring-up. Fails closed on any step: standby + RF_SW low + IRQs
 * masked, never left mid-configured. */
esp_err_t lora_radio_rx_start(const lora_rx_params_t *params)
{
    if (!params) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_lock, portMAX_DELAY);

    /* A new listener owns a new evidence window. At this point the previous
     * drain task is known idle by lora_recon.c, so no old event can leak into
     * the reset counter window. */
    memset((void *)&s_stats, 0, sizeof s_stats);
    xQueueReset(s_dio1_queue);
    xQueueReset(s_event_queue);

    esp_err_t err = hw_reset();

    uint8_t standby_rc = STDBY_RC;
    if (err == ESP_OK) err = cmd_write(OP_SET_STANDBY, &standby_rc, 1);

    /* TCXO (D-10) before calibration: the chip needs its real clock running
     * to calibrate meaningfully. */
    if (err == ESP_OK) {
        uint8_t tcxo[4] = {
            TCXO_VOLTAGE_1V8,
            (uint8_t)(TCXO_DELAY_10MS >> 16), (uint8_t)(TCXO_DELAY_10MS >> 8), (uint8_t)TCXO_DELAY_10MS,
        };
        err = cmd_write(OP_SET_DIO3_TCXO_CTRL, tcxo, sizeof tcxo);
    }

    if (err == ESP_OK) {
        uint8_t calib_all = 0x7F;   /* all blocks, Table 13-18 */
        err = cmd_write(OP_CALIBRATE, &calib_all, 1);
    }
    if (err == ESP_OK) err = wait_busy_low();   /* calibration itself, ~3.5ms typ */

    /* Enter STDBY_XOSC now so the 10ms TCXO delay is paid once here, not on
     * every RX entry (datasheet §13.3.6's own recommended technique). */
    if (err == ESP_OK) {
        uint8_t standby_xosc = STDBY_XOSC;
        err = cmd_write(OP_SET_STANDBY, &standby_xosc, 1);
    }

    /* D-10: XOSC_START_ERR is *expected* here on a cold start with a TCXO
     * (datasheet §13.3.6's own note) — the chip doesn't yet know it's
     * TCXO-clocked until this point. This isn't a pass/fail gate, it's the
     * positive confirmation D-10 was missing: the TCXO actually started
     * within our chosen 10ms delay, not just inferred from nothing else
     * faulting. Non-fatal either way — logged, then cleared per the
     * datasheet's explicit instruction ("simply clear this flag with
     * ClearDeviceErrors"), never blocks bring-up. */
    if (err == ESP_OK) {
        uint8_t errs_raw[2];
        if (cmd_read(OP_GET_DEVICE_ERRORS, errs_raw, sizeof errs_raw) == ESP_OK) {
            uint16_t op_err = ((uint16_t)errs_raw[0] << 8) | errs_raw[1];
            if (op_err & OPERR_XOSC_START_ERR) {
                ESP_LOGI(TAG, "XOSC_START_ERR set at cold start (expected) — clearing");
            }
            if (op_err & ~(uint16_t)OPERR_XOSC_START_ERR) {
                ESP_LOGW(TAG, "unexpected device error bits: 0x%04x", op_err);
            }
        }
        uint8_t clear_errs[2] = { 0x00, 0x00 };
        cmd_write(OP_CLEAR_DEVICE_ERRORS, clear_errs, sizeof clear_errs);
    }

    if (err == ESP_OK) {
        uint8_t dc_dc = 0x01;   /* Rev D: "use the module's DC-DC regulator mode" */
        err = cmd_write(OP_SET_REGULATOR_MODE, &dc_dc, 1);
    }

    /* Let the chip drive its internal DIO2 automatically; the external
     * RF_SW GPIO still needs to be driven separately (Rev D §4.4 — enabling
     * DIO2 alone does not manage it). */
    if (err == ESP_OK) {
        uint8_t enable = 0x01;
        err = cmd_write(OP_SET_DIO2_RF_SWITCH, &enable, 1);
    }

    if (err == ESP_OK) {
        uint8_t packet_type = PACKET_TYPE_LORA;
        err = cmd_write(OP_SET_PACKET_TYPE, &packet_type, 1);
    }
    if (err == ESP_OK) err = lora_set_rf_frequency(params->freq_hz);
    if (err == ESP_OK) {
        uint8_t base_addr[2] = { 0x00, 0x00 };
        err = cmd_write(OP_SET_BUFFER_BASE_ADDR, base_addr, sizeof base_addr);
    }
    if (err == ESP_OK) err = lora_bw_cr_sf_to_modparams(params);
    if (err == ESP_OK) err = lora_set_packet_params();

    if (err == ESP_OK) {
        /* IrqMask, DIO1Mask, DIO2Mask, DIO3Mask — DIO2/3 are the RF switch
         * and TCXO, not IRQ sources here, so their masks stay zero. */
        uint8_t irq[8] = {
            (uint8_t)(IRQ_RX_MASK >> 8), (uint8_t)IRQ_RX_MASK,
            (uint8_t)(IRQ_RX_MASK >> 8), (uint8_t)IRQ_RX_MASK,
            0x00, 0x00,
            0x00, 0x00,
        };
        err = cmd_write(OP_SET_DIO_IRQ_PARAMS, irq, sizeof irq);
    }
    if (err == ESP_OK) {
        uint8_t clear_all[2] = { 0xFF, 0xFF };
        err = cmd_write(OP_CLEAR_IRQ_STATUS, clear_all, sizeof clear_all);
    }

    if (err == ESP_OK) {
        rf_switch_set_receive(true);
        uint8_t rx_continuous[3] = { 0xFF, 0xFF, 0xFF };
        err = cmd_write(OP_SET_RX, rx_continuous, sizeof rx_continuous);
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rx_start failed: %s", esp_err_to_name(err));
        rf_switch_set_receive(false);
        uint8_t standby_rc2 = STDBY_RC;
        cmd_write(OP_SET_STANDBY, &standby_rc2, 1);
        s_running = false;
    } else {
        s_running = true;
    }

    xSemaphoreGive(s_lock);
    return err;
}

void lora_radio_rx_stop(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_running) {
        uint8_t standby_rc = STDBY_RC;
        cmd_write(OP_SET_STANDBY, &standby_rc, 1);
        rf_switch_set_receive(false);
        s_running = false;
    }
    xSemaphoreGive(s_lock);
}

bool lora_radio_is_running(void)
{
    return s_running;
}

bool lora_radio_next_event(lora_event_t *out, uint32_t timeout_ms)
{
    return xQueueReceive(s_event_queue, out, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void lora_radio_get_stats(lora_radio_stats_t *out)
{
    if (!out) return;
    /* Word-sized counters are an observational snapshot, not a lock-free
     * transaction. A simultaneous ISR/task increment can only move a value
     * forward in the next status response. */
    out->rx = s_stats.rx;
    out->crc_err = s_stats.crc_err;
    out->header_err = s_stats.header_err;
    out->irq_drop = s_stats.irq_drop;
    out->event_drop = s_stats.event_drop;
    out->hw_fault = s_stats.hw_fault;
}

/* ---- DIO1 ISR + radio task ------------------------------------------------ */

static void IRAM_ATTR dio1_isr(void *arg)
{
    (void)arg;
    uint8_t tag = 1;
    BaseType_t woken = pdFALSE;
    if (xQueueSendFromISR(s_dio1_queue, &tag, &woken) != pdTRUE) s_stats.irq_drop++;
    if (woken) portYIELD_FROM_ISR();
}

static void handle_rx_done(void)
{
    uint8_t buf_status[2];
    if (cmd_read(OP_GET_RX_BUFFER_STATUS, buf_status, sizeof buf_status) != ESP_OK) return;
    uint8_t len = buf_status[0];   /* uint8_t already can't exceed LORA_MAX_PAYLOAD (255) */
    uint8_t offset = buf_status[1];

    lora_event_t evt = { .kind = LORA_EVT_PACKET };
    if (read_buffer(offset, evt.packet.payload, len) != ESP_OK) return;
    evt.packet.len = len;

    uint8_t pkt_status[3];
    if (cmd_read(OP_GET_PACKET_STATUS, pkt_status, sizeof pkt_status) == ESP_OK) {
        evt.packet.rssi_dbm = (int16_t)(-(int16_t)pkt_status[0] / 2);
        evt.packet.snr_db = (float)(int8_t)pkt_status[1] / 4.0f;
    }
    s_stats.rx++;
    if (xQueueSend(s_event_queue, &evt, 0) != pdTRUE) s_stats.event_drop++;
}

static void queue_radio_event(lora_evt_kind_t kind)
{
    lora_event_t evt = { .kind = kind };
    if (xQueueSend(s_event_queue, &evt, 0) != pdTRUE) s_stats.event_drop++;
}

static void lora_task(void *arg)
{
    (void)arg;
    uint8_t tag;
    for (;;) {
        if (xQueueReceive(s_dio1_queue, &tag, portMAX_DELAY) != pdTRUE) continue;

        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (!s_running) { xSemaphoreGive(s_lock); continue; }

        /* IrqStatus(15:0) is 2 payload bytes after RFU+Status (datasheet
         * Table 13-30) — not 3. A too-long read here previously clocked one
         * spurious byte and reconstructed `irq` from the wrong two bytes,
         * which fed a corrupted value into ClearIrqStatus below. On the
         * wrong bit combination that left the real fired IRQ bit uncleared
         * in the chip, and since DIO1 is edge-triggered, a stuck-set bit
         * means no further rising edge ever comes — a silent, permanent RX
         * stall with no error anywhere. Root-caused via a 2h soak
         * cross-referenced against an independent observer (WORKLOG
         * 2026-09-13, docs/hardware/lora-harness.md). */
        uint8_t irq_raw[2];
        if (cmd_read(OP_GET_IRQ_STATUS, irq_raw, sizeof irq_raw) != ESP_OK) {
            /* Can't know what fired, so nothing below can safely run this
             * cycle. Counted, not just logged: a wedged bus here reproduces
             * the same symptom as the original silent DIO1 stall
             * (lora-harness.md) — an evidence gap this closes. */
            s_stats.hw_fault++;
            xSemaphoreGive(s_lock);
            continue;
        }
        uint16_t irq = ((uint16_t)irq_raw[0] << 8) | irq_raw[1];

        uint8_t clear[2] = { (uint8_t)(irq >> 8), (uint8_t)irq };
        if (cmd_write(OP_CLEAR_IRQ_STATUS, clear, sizeof clear) != ESP_OK) s_stats.hw_fault++;

        if (irq & IRQ_RX_DONE) {
            if (!(irq & (IRQ_CRC_ERR | IRQ_HEADER_ERR))) handle_rx_done();
        }
        if (irq & IRQ_TIMEOUT) {
            queue_radio_event(LORA_EVT_TIMEOUT);
        }
        if (irq & IRQ_CRC_ERR) {
            s_stats.crc_err++;
            queue_radio_event(LORA_EVT_CRC_ERR);
        }
        if (irq & IRQ_HEADER_ERR) {
            s_stats.header_err++;
            queue_radio_event(LORA_EVT_HEADER_ERR);
        }

        xSemaphoreGive(s_lock);
    }
}
