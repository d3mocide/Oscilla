/*
 * cc1101_radio.c — see cc1101_radio.h.
 *
 * Register/strobe addresses and formulas are transcribed from the TI CC1101
 * datasheet (SWRS061), not guessed or lifted from a generic library — the
 * same discipline lora_radio.c holds itself to for the SX1262. Register
 * *values* are computed by cc1101_regs.c, not hand-picked — a hand-picked
 * profile is what produced a real bug (WORKLOG 2026-09-21/22): the wrong
 * data rate and a receive filter 4x wider than intended.
 *
 * Front-end and TEST0-2 registers stay at power-on-reset defaults (no
 * unverified "recommended settings" copied from elsewhere); AGCCTRL1 is
 * the one exception — see cc1101_set_rx_profile() and WORKLOG.
 *
 * Locking: s_lock guards s_running against concurrent rx_start/rx_stop/
 * is_running calls from the dispatch task while the poll task is mid-drain.
 *
 * No SPI inside an ISR: there is no ISR here at all (GDO0 is unconnected in
 * this harness revision — wiring-doc §4.1), so every SPI access happens on
 * either the dispatch task (rx_start/rx_stop) or the poll task, never both
 * without the lock held.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_radio.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "cc1101_regs.h"

static const char *TAG = "cc1101_radio";

/* Wiring-doc §2/§4 pin map. SCK/MISO/MOSI are the same physical pins
 * lora_radio.c already configured and initialized the SPI2 bus on. */
#define PIN_SCK    8
#define PIN_MISO   9
#define PIN_MOSI   10
#define PIN_CS     7

#define SPI_HOST_ID     SPI2_HOST
#define SPI_INITIAL_HZ  1000000   /* wiring-doc §4.3: start ~1 MHz, shared bus */

#define MISO_TIMEOUT_MS 200   /* generous margin over the crystal's real startup */
/* At 26 kbaud a full 64-byte FIFO can fill in ~19.7ms continuously — 10ms
 * leaves comfortable margin so a real (or still-noisy) burst can't
 * silently overflow between polls. */
#define POLL_PERIOD_MS  10
#define RXBYTES_RECHECK_US 200   /* gap between the two stable-read RXBYTES samples */

#define FXOSC_HZ  26000000UL   /* bench-assumed, not datasheet-confirmed for this module — see WORKLOG */

/* Chosen profile, 2026-09-22: 26000 baud target (achieves ~25985), 203125 Hz
 * bandwidth (the chip's own POR default, comfortably >4x the data rate).
 * The baud target isn't the modulation rate of any known device — it's a
 * deliberate ~6x oversample of a real, SDR-confirmed local sensor's
 * shortest PWM pulse (232us, LaCrosse-TX141THBv2 — see WORKLOG), so pulse
 * widths can be reconstructed from run-lengths in the raw captured bits
 * (tools/legacy_pulse.py) rather than decoded as fixed-period NRZ bits,
 * which CC1101's synchronous demodulator can't do for PWM-encoded sources
 * on its own. Not a general-purpose data rate — see cc1101_set_rx_profile(). */
#define TARGET_BAUD_HZ    26000UL
#define TARGET_CHANBW_HZ  203125UL

/* Config register addresses, datasheet Table 44. Only the ones this driver
 * actually writes get a name; everything else stays at chip POR default. */
#define REG_IOCFG1    0x01
#define REG_PKTCTRL0  0x08
#define REG_FREQ2     0x0D
#define REG_FREQ1     0x0E
#define REG_FREQ0     0x0F
#define REG_MDMCFG4   0x10
#define REG_MDMCFG3   0x11
#define REG_MDMCFG2   0x12
#define REG_MCSM1     0x17
#define REG_AGCCTRL1  0x1C

/* Status registers, datasheet Table 45 — status registers require the burst
 * bit set even for a single byte (§10.4's documented quirk: without it, the
 * same address range aliases onto the command-strobe table instead). */
#define REG_PARTNUM   0x30
#define REG_VERSION   0x31
#define REG_RSSI      0x34
#define REG_RXBYTES   0x3B

/* RSSI_offset (datasheet §17.3, Table 31): 74 dB at every listed data rate
 * (1.2/38.4/250/500 kBaud) and both 433/868 MHz bands — verified directly
 * against the table, not assumed constant across profile changes. */
#define RSSI_OFFSET_DB 74

#define REG_FIFO      0x3F

/* Command strobes, datasheet Table 42. TX-shaped strobes (SFSTXON=0x35,
 * STX=0x35... never referenced — reserved names live in
 * tools/check_rx_only.py's PROBE_BANNED so they can't silently appear. */
#define STROBE_SRES   0x30
#define STROBE_SIDLE  0x36
#define STROBE_SRX    0x34
#define STROBE_SFRX   0x3A   /* flush RX FIFO; only valid from IDLE */

#define HDR_READ         0x80
#define HDR_BURST        0x40

/* MDMCFG2 fields (datasheet Table 49). MOD_FORMAT is bits [6:4]; SYNC_MODE
 * is bits [2:0] — 100 is "no preamble/sync word, carrier-sense above
 * threshold" (as opposed to 000, "no preamble/sync", which has no gating at
 * all and reports every demodulated bit regardless of signal presence).
 * Carrier-sense doesn't require knowing any device's real sync word, unlike
 * the 15/16- or 16/16-bit sync-match modes — appropriate here since we
 * don't know one. */
#define MOD_FORMAT_OOK        (0x3 << 4)
#define SYNC_MODE_CARRIER_SENSE  0x4

/* AGCCTRL1 register 0x1C (datasheet §17.4/§29.3) — address re-verified
 * against the datasheet's own register table after a wrong-register bug,
 * see WORKLOG. bit6 AGC_LNA_PRIORITY (POR default 1, left as-is), bits[5:4]
 * CARRIER_SENSE_REL_THR=6dB (0x58), the least strict of the three built-in
 * options, bits[3:0] CARRIER_SENSE_ABS_THR=1000 (-8, disabled, so only the
 * relative condition gates). Neither 6dB nor 14dB caught a real,
 * SDR-confirmed transmission on the bench (2026-09-22, WORKLOG) — not yet
 * known to be the right value, or even the right mechanism; see rssi_diag's
 * findings below. */
#define AGCCTRL1_VALUE  0x58

/* RXBYTES' low 7 bits are the FIFO occupancy; bit7 set is the overflow flag
 * (datasheet §10.4). */
#define RXBYTES_OVERFLOW (1u << 7)

static spi_device_handle_t s_spi;
static SemaphoreHandle_t s_lock;
static SemaphoreHandle_t s_poll_sem;   /* parks poll_task until a session is running */
static QueueHandle_t s_event_queue;
static TaskHandle_t s_task;
static volatile bool s_running;
static uint32_t s_fifo_overflow_count;
static uint32_t s_queue_drop_count;

static void cs_select(void)   { gpio_set_level(PIN_CS, 0); }
static void cs_deselect(void) { gpio_set_level(PIN_CS, 1); }

/* Bounded: SO (shared MISO pin) must fall before the chip is ready — fault,
 * never hang (Rev D §4.3's rule, same one lora_radio.c's wait_busy_low()
 * and the CC1101 CSn bring-up already commit to). Read directly, not
 * through the SPI peripheral, same trick lora_radio.c uses for BUSY. */
static esp_err_t wait_miso_low(void)
{
    int64_t deadline = esp_timer_get_time() + (int64_t)MISO_TIMEOUT_MS * 1000;
    while (gpio_get_level(PIN_MISO)) {
        if (esp_timer_get_time() > deadline) {
            ESP_LOGE(TAG, "SO stuck high past %d ms", MISO_TIMEOUT_MS);
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_OK;
}

/* No wait_miso_low() precondition: SO only signals readiness right after a
 * CS-low edge (datasheet §19.1), not on an otherwise-idle shared bus —
 * see WORKLOG for the spurious-timeout bug this distinction fixed. */
static esp_err_t strobe(uint8_t opcode)
{
    uint8_t tx[1] = { opcode };
    spi_transaction_t t = { .length = 8, .tx_buffer = tx };
    cs_select();
    esp_err_t err = spi_device_transmit(s_spi, &t);
    cs_deselect();
    return err;
}

static esp_err_t write_reg(uint8_t addr, uint8_t value)
{
    uint8_t tx[2] = { (uint8_t)(addr & 0x3F), value };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx };
    cs_select();
    esp_err_t err = spi_device_transmit(s_spi, &t);
    cs_deselect();
    return err;
}

/* Status-register burst read (§10.4's quirk — see REG_PARTNUM comment). */
static esp_err_t read_status(uint8_t addr, uint8_t *out)
{
    uint8_t tx[2] = { (uint8_t)((addr & 0x3F) | HDR_READ | HDR_BURST), 0x00 };
    uint8_t rx[2] = { 0 };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx, .rx_buffer = rx };
    cs_select();
    esp_err_t err = spi_device_transmit(s_spi, &t);
    cs_deselect();
    if (err == ESP_OK && out) *out = rx[1];
    return err;
}

static esp_err_t read_fifo(uint8_t *out, size_t len)
{
    if (len > CC1101_MAX_CHUNK) return ESP_ERR_INVALID_SIZE;

    uint8_t tx[1 + CC1101_MAX_CHUNK] = { (uint8_t)(REG_FIFO | HDR_READ | HDR_BURST) };
    uint8_t rx[1 + CC1101_MAX_CHUNK] = { 0 };
    spi_transaction_t t = { .length = (1 + len) * 8, .tx_buffer = tx, .rx_buffer = rx };
    cs_select();
    esp_err_t err = spi_device_transmit(s_spi, &t);
    cs_deselect();
    if (err == ESP_OK) memcpy(out, &rx[1], len);
    return err;
}

/* ---- init / reset -------------------------------------------------------- */

static void poll_task(void *arg);

esp_err_t cc1101_radio_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;

    s_poll_sem = xSemaphoreCreateBinary();   /* starts empty: poll_task parks until rx_start gives it */
    if (!s_poll_sem) return ESP_ERR_NO_MEM;

    s_event_queue = xQueueCreate(8, sizeof(cc1101_event_t));
    if (!s_event_queue) return ESP_ERR_NO_MEM;

    const gpio_config_t cs_cfg = { .pin_bit_mask = 1ULL << PIN_CS, .mode = GPIO_MODE_OUTPUT };
    esp_err_t err = gpio_config(&cs_cfg);
    if (err != ESP_OK) return err;
    cs_deselect();

    /* SCK/MISO/MOSI already belong to the shared SPI2 bus — lora_radio_init()
     * runs first (main.c) and owns spi_bus_initialize(); this only adds a
     * second device. ESP_ERR_INVALID_STATE means the bus is already up,
     * which is the expected, not-an-error case when both radios attach. */
    const spi_bus_config_t bus_cfg = {
        .sclk_io_num = PIN_SCK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CC1101_MAX_CHUNK + 8,
    };
    err = spi_bus_initialize(SPI_HOST_ID, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    const spi_device_interface_config_t dev_cfg = {
        .mode = 0,   /* CPOL=0, CPHA=0, datasheet §8.2 SPI timing */
        .clock_speed_hz = SPI_INITIAL_HZ,
        .spics_io_num = -1,   /* CSn driven manually so it can be shared with the Wio's own CS */
        .queue_size = 1,
    };
    err = spi_bus_add_device(SPI_HOST_ID, &dev_cfg, &s_spi);
    if (err != ESP_OK) return err;

    return xTaskCreate(poll_task, "cc1101_poll", 4096, NULL, 4, &s_task) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

/* Wiring-doc §2's manual reset, CS held low across select->SO-wait->SRES->
 * SO-wait. Skips the doc's literal SCK/SI bit-bang (would fight the Wio's
 * use of the same shared-bus pins) — see WORKLOG. */
static esp_err_t hw_reset(void)
{
    cs_select();
    esp_rom_delay_us(5);
    cs_deselect();
    esp_rom_delay_us(40);   /* datasheet §19.1: wait >=40us before the real select */
    cs_select();
    esp_err_t err = wait_miso_low();

    /* Sent directly, not via strobe(): that helper does its own
     * select/deselect, which would drop CS between this strobe and the
     * SO-low wait right after it — both must happen under one continuous
     * CS-low span per the datasheet's reset sequence. */
    if (err == ESP_OK) {
        uint8_t tx[1] = { STROBE_SRES };
        spi_transaction_t t = { .length = 8, .tx_buffer = tx };
        err = spi_device_transmit(s_spi, &t);
    }
    if (err == ESP_OK) err = wait_miso_low();
    cs_deselect();
    return err;
}

/* ---- modem bring-up ------------------------------------------------------- */

static esp_err_t cc1101_set_rf_frequency(uint32_t freq_hz)
{
    uint32_t freq_reg = cc1101_freq_reg(freq_hz, FXOSC_HZ);
    esp_err_t err = write_reg(REG_FREQ2, (uint8_t)(freq_reg >> 16));
    if (err == ESP_OK) err = write_reg(REG_FREQ1, (uint8_t)(freq_reg >> 8));
    if (err == ESP_OK) err = write_reg(REG_FREQ0, (uint8_t)freq_reg);
    return err;
}

/* Carrier-sense-gated (no literal sync word — per-sensor preambles are
 * unknown, but free-running with SYNC_MODE=0 reported continuous noise as
 * if it were data, not just silence between real bursts; see WORKLOG),
 * infinite packet length (LENGTH_CONFIG=10, datasheet §8) so a real
 * burst's demodulated bytes just keep filling the FIFO for the poll task
 * to drain, no CRC/whitening (an external sensor doesn't speak CC1101's
 * own framing). Data rate and bandwidth are computed by cc1101_regs.c from
 * TARGET_BAUD_HZ/TARGET_CHANBW_HZ, not hand-picked. Not asserted correct
 * for any specific sensor — a starting point to tune once real captures
 * exist (same stance as D-10). */
static esp_err_t cc1101_set_rx_profile(void)
{
    uint8_t drate_e, drate_m, chanbw_e, chanbw_m;
    uint32_t achieved_baud = cc1101_drate_reg(TARGET_BAUD_HZ, FXOSC_HZ, &drate_e, &drate_m);
    uint32_t achieved_bw = cc1101_chanbw_reg(TARGET_CHANBW_HZ, FXOSC_HZ, &chanbw_e, &chanbw_m);
    ESP_LOGI(TAG, "rx profile: target=%luBd/%luHz achieved=%luBd/%luHz",
             (unsigned long)TARGET_BAUD_HZ, (unsigned long)TARGET_CHANBW_HZ,
             (unsigned long)achieved_baud, (unsigned long)achieved_bw);

    uint8_t mdmcfg4 = (uint8_t)((chanbw_e << 6) | (chanbw_m << 4) | drate_e);
    esp_err_t err = write_reg(REG_MDMCFG4, mdmcfg4);
    if (err == ESP_OK) err = write_reg(REG_MDMCFG3, drate_m);
    if (err == ESP_OK) {
        err = write_reg(REG_MDMCFG2, MOD_FORMAT_OOK | SYNC_MODE_CARRIER_SENSE);
    }
    if (err == ESP_OK) err = write_reg(REG_AGCCTRL1, AGCCTRL1_VALUE);
    if (err == ESP_OK) err = write_reg(REG_PKTCTRL0, 0x02);   /* normal FIFO, no CRC, infinite length */
    if (err == ESP_OK) err = write_reg(REG_MCSM1, 0x0C);      /* RXOFF_MODE=stay in RX */
    /* Three-state GDO1 while CSn is high elsewhere on the shared bus
     * (wiring-doc §5.2's explicit requirement). */
    if (err == ESP_OK) err = write_reg(REG_IOCFG1, 0x2E);
    return err;
}

esp_err_t cc1101_radio_rx_start(const cc1101_rx_params_t *params)
{
    if (!params) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_lock, portMAX_DELAY);

    esp_err_t err = hw_reset();
    if (err == ESP_OK) err = cc1101_set_rf_frequency(params->freq_hz);
    if (err == ESP_OK) err = cc1101_set_rx_profile();
    if (err == ESP_OK) err = strobe(STROBE_SFRX);   /* flush any stale FIFO content */
    if (err == ESP_OK) err = strobe(STROBE_SRX);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rx_start failed: %s", esp_err_to_name(err));
        strobe(STROBE_SIDLE);
        s_running = false;
    } else {
        /* A stopped session's queued-but-undrained events must not surface
         * as if they belonged to this new one (WORKLOG 2026-09-21/22). */
        xQueueReset(s_event_queue);
        s_fifo_overflow_count = 0;
        s_queue_drop_count = 0;
        s_running = true;
        xSemaphoreGive(s_poll_sem);   /* wake poll_task, parked since the last stop (or boot) */
    }

    xSemaphoreGive(s_lock);
    return err;
}

void cc1101_radio_rx_stop(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_running) {
        strobe(STROBE_SIDLE);
        s_running = false;
    }
    xSemaphoreGive(s_lock);
}

bool cc1101_radio_is_running(void)
{
    return s_running;
}

bool cc1101_radio_next_event(cc1101_event_t *out, uint32_t timeout_ms)
{
    return xQueueReceive(s_event_queue, out, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void cc1101_radio_get_counters(uint32_t *fifo_overflows, uint32_t *queue_drops)
{
    if (fifo_overflows) *fifo_overflows = s_fifo_overflow_count;
    if (queue_drops) *queue_drops = s_queue_drop_count;
}

/* ---- probe (PARTNUM/VERSION) ---------------------------------------------- */

esp_err_t cc1101_radio_read_id(uint8_t *partnum, uint8_t *version)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t err = hw_reset();
    if (err == ESP_OK) err = read_status(REG_PARTNUM, partnum);
    if (err == ESP_OK) err = read_status(REG_VERSION, version);
    strobe(STROBE_SIDLE);
    xSemaphoreGive(s_lock);
    return err;
}

/* ---- poll task ------------------------------------------------------------- */

static void drain_fifo(void)
{
    uint8_t rxbytes1 = 0;
    if (read_status(REG_RXBYTES, &rxbytes1) != ESP_OK) return;

    if (rxbytes1 & RXBYTES_OVERFLOW) {
        s_fifo_overflow_count++;
        strobe(STROBE_SIDLE);
        strobe(STROBE_SFRX);
        strobe(STROBE_SRX);
        cc1101_event_t evt = { .kind = CC1101_EVT_OVERFLOW };
        if (xQueueSend(s_event_queue, &evt, 0) != pdTRUE) s_queue_drop_count++;
        return;
    }

    /* Stable-read guard against the datasheet's documented race (a read
     * landing mid-transfer of the last FIFO byte can return a stale
     * count): two RXBYTES reads a short, bounded gap apart, then
     * cc1101_drain_count() decides how much is safe to pull now. */
    esp_rom_delay_us(RXBYTES_RECHECK_US);
    uint8_t rxbytes2 = 0;
    if (read_status(REG_RXBYTES, &rxbytes2) != ESP_OK) return;
    if (rxbytes2 & RXBYTES_OVERFLOW) return;   /* handle it on the next poll, not mid-decision here */

    int n = cc1101_drain_count(rxbytes1, rxbytes2, CC1101_MAX_CHUNK);
    if (n <= 0) return;

    cc1101_event_t evt = { .kind = CC1101_EVT_CHUNK };
    if (read_fifo(evt.chunk.payload, (size_t)n) != ESP_OK) return;
    evt.chunk.len = (uint8_t)n;
    uint8_t rssi_raw = 0;
    evt.chunk.rssi_dbm = (read_status(REG_RSSI, &rssi_raw) == ESP_OK)
                              ? cc1101_rssi_to_dbm(rssi_raw, RSSI_OFFSET_DB) : 0;
    if (xQueueSend(s_event_queue, &evt, 0) != pdTRUE) s_queue_drop_count++;
}

/* Bench diagnostic, added 2026-09-22 (WORKLOG): RSSI is normally only
 * reported alongside a captured chunk, so when carrier-sense never opens
 * the FIFO there's no RSSI data at all to look at. REG_RSSI itself updates
 * continuously whenever the receiver is active (datasheet §17.3),
 * independent of the digital sync/carrier-sense gating that controls FIFO
 * writes, so this reads it unconditionally every poll tick and reports
 * last/peak once a second — used to correlate against SDR-confirmed real
 * transmission timestamps (found no measurable bump at any of five
 * checked, plus an unexplained baseline drift; see WORKLOG). Kept as
 * standing instrumentation, not reverted — the question it answers isn't
 * resolved yet. */
#define RSSI_DIAG_REPORT_MS  1000
static int s_rssi_diag_peak_dbm = -128;
static uint32_t s_rssi_diag_last_report_ms;

static void rssi_diag_tick(void)
{
    uint8_t rssi_raw = 0;
    if (read_status(REG_RSSI, &rssi_raw) != ESP_OK) return;
    int dbm = cc1101_rssi_to_dbm(rssi_raw, RSSI_OFFSET_DB);
    if (dbm > s_rssi_diag_peak_dbm) s_rssi_diag_peak_dbm = dbm;

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if (now_ms - s_rssi_diag_last_report_ms >= RSSI_DIAG_REPORT_MS) {
        ESP_LOGI(TAG, "rssi_diag last=%ddBm peak=%ddBm", dbm, s_rssi_diag_peak_dbm);
        s_rssi_diag_peak_dbm = -128;
        s_rssi_diag_last_report_ms = now_ms;
    }
}

/* Parked on s_poll_sem until a session actually starts, rather than
 * waking every POLL_PERIOD_MS from boot regardless of use — this is a
 * permanently-attached driver on a single-core chip, and CC1101 may never
 * be used in a given session. */
static void poll_task(void *arg)
{
    (void)arg;
    for (;;) {
        xSemaphoreTake(s_poll_sem, portMAX_DELAY);
        while (s_running) {
            vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));
            xSemaphoreTake(s_lock, portMAX_DELAY);
            if (s_running) {
                rssi_diag_tick();
                drain_fifo();
            }
            xSemaphoreGive(s_lock);
        }
    }
}
