/*
 * cc1101_bench.c — see cc1101_bench.h. Output lines (bench-cc1101-driver
 * README): I, D, R, S, E. GDO0 has no C5 pin, so the edge column is 0.
 *
 * SPDX-License-Identifier: MIT
 */

#include "cc1101_bench.h"
#include "sdkconfig.h"

#if CONFIG_OSCILLA_CC1101_BENCH

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cc1101_bus.h"
#include "cc1101_config.h"
#include "cc1101_port.h"
#include "cc1101_profile.h"
#include "cc1101_rx.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lora_radio.h"
#include "lora_recon.h"

#define WINDOW_MS 20
#define VERIFY_MS 1000
#define LLOAD_LO_HZ 902000000UL
#define LLOAD_HI_HZ 928000000UL

static cc1101_bus_t s_bus;
static cc1101_profile_t s_profile;
static uint32_t s_gap_us = CC1101_RSSI_POLL_MIN_US;
static bool s_quiet;
static uint32_t s_drifts;
static char s_bringup[200];

static volatile bool s_lload;
static volatile uint32_t s_lload_sweeps, s_lload_errors;
static TaskHandle_t s_lload_task;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void out(const char *fmt, ...)
{
    char line[320];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n > 0) usb_serial_jtag_write_bytes(line, n < (int)sizeof line ? (size_t)n : sizeof line - 1,
                                           pdMS_TO_TICKS(50));
}

static void bring_up(bool do_reset)
{
    bool edge = false, cal = false;
    cc1101_apply_report_t r = { 0, 0, 0, -1 };
    const cc1101_err_t pr = cc1101_probe(&s_bus, NULL, NULL);
    const cc1101_err_t rs = do_reset ? cc1101_reset(&s_bus, &edge) : CC1101_OK;
    const cc1101_err_t ap = cc1101_apply_profile(&s_bus, &s_profile, &r);
    const cc1101_err_t rx = ap == CC1101_OK ? cc1101_rx_start(&s_bus, &cal) : ap;
    snprintf(s_bringup, sizeof s_bringup,
             "probe=%s,reset=%s,so_edge=%d,apply=%s,written=%u,retried=%u,failed=%u,first_failed=%d,"
             "rx=%s,calibrated=%d",
             cc1101_err_name(pr), do_reset ? cc1101_err_name(rs) : "skipped", edge,
             cc1101_err_name(ap), r.written, r.retried, r.failed, r.first_failed,
             cc1101_err_name(rx), cal);
}

static void print_info(void)
{
    uint8_t part = 0, ver = 0, f2 = 0, f1 = 0, f0 = 0;
    const cc1101_err_t perr = cc1101_probe(&s_bus, &part, &ver);
    cc1101_read_reg(&s_bus, CC1101_FREQ2, &f2);
    cc1101_read_reg(&s_bus, CC1101_FREQ1, &f1);
    cc1101_read_reg(&s_bus, CC1101_FREQ0, &f0);
    const uint32_t freq = ((uint32_t)f2 << 16) | ((uint32_t)f1 << 8) | f0;
    out("I,partnum=0x%02x,version=0x%02x,freq_reg=0x%06lx,mhz_at_26=%.4f,probe=%s,drifts=%lu,"
        "lload=%d,lload_sweeps=%lu,lload_errors=%lu,driver=oscilla-c5,%s\n",
        part, ver, (unsigned long)freq, freq * 26.0 / 65536.0, cc1101_err_name(perr),
        (unsigned long)s_drifts, s_lload, (unsigned long)s_lload_sweeps,
        (unsigned long)s_lload_errors, s_bringup);
}

static void dump_regs(void)
{
    char line[320];
    int n = snprintf(line, sizeof line, "D");
    for (uint8_t a = 0; a < CC1101_CFG_COUNT && n < (int)sizeof line - 8; a++) {
        uint8_t v = 0;
        cc1101_read_reg(&s_bus, a, &v);
        n += snprintf(line + n, sizeof line - n, ",%02x=%02x", a, v);
    }
    out("%s\n", line);
}

static void stress(bool full, long n)
{
    long fail = 0, no_edge = 0, retried = 0;
    char first[96] = "";
    const uint32_t t0 = now_ms();
    for (long i = 0; i < n; i++) {
        cc1101_err_t err;
        bool cal = false;
        if (full) {
            bool edge = false;
            cc1101_apply_report_t r = { 0, 0, 0, -1 };
            err = cc1101_reset(&s_bus, &edge);
            if (err == CC1101_OK) err = cc1101_apply_profile(&s_bus, &s_profile, &r);
            if (err == CC1101_OK) err = cc1101_rx_start(&s_bus, &cal);
            no_edge += !edge;
            retried += r.retried;
            if (err != CC1101_OK && !first[0])
                snprintf(first, sizeof first, "i=%ld,err=%s,failed=%u,first_failed=%d", i,
                         cc1101_err_name(err), r.failed, r.first_failed);
        } else {
            err = cc1101_rx_stop(&s_bus);
            if (err == CC1101_OK) err = cc1101_rx_start(&s_bus, &cal);
            if (err != CC1101_OK && !first[0])
                snprintf(first, sizeof first, "i=%ld,err=%s", i, cc1101_err_name(err));
        }
        if (err == CC1101_OK && !cal) err = CC1101_ERR_STATE;
        fail += err != CC1101_OK;
        if ((i & 15) == 0) vTaskDelay(1);   /* single core: never starve (AGENTS gotcha 6) */
    }
    int16_t at = -1;
    uint8_t marc = 0;
    const cc1101_err_t v = cc1101_verify_profile(&s_bus, &s_profile, &at);
    cc1101_read_status(&s_bus, CC1101_MARCSTATE, &marc);
    out("S,%s,n=%ld,fail=%ld,no_so_edge=%ld,reg_retries=%ld,ms=%lu,verify=%s,marc=%02x,first_fail=%s\n",
        full ? "bringup" : "rxcycle", n, fail, no_edge, retried, (unsigned long)(now_ms() - t0),
        cc1101_err_name(v), marc, first[0] ? first : "none");
    if (v != CC1101_OK || marc != CC1101_MARC_RX) {
        bring_up(false);
        print_info();
    }
}

/* SX1262 receive-only RSSI sweep, as shared-bus load for the coexistence test. */
static void lload_task(void *arg)
{
    (void)arg;
    uint32_t f = LLOAD_LO_HZ;
    while (s_lload) {
        int8_t dbm = 0;
        if (lora_radio_sweep_measure(f, 8, &dbm) == ESP_OK) s_lload_sweeps++;
        else s_lload_errors++;
        f = f + 1000000UL > LLOAD_HI_HZ ? LLOAD_LO_HZ : f + 1000000UL;
        vTaskDelay(1);
    }
    lora_radio_sweep_stop();
    s_lload_task = NULL;
    vTaskDelete(NULL);
}

static void lload(bool on)
{
    if (on && !s_lload) {
        if (!lora_recon_ready()) { out("E,lload,lora_unavailable\n"); return; }
        if (lora_radio_is_running()) { out("E,lload,lora_rx_running\n"); return; }
        const esp_err_t err = lora_radio_sweep_start(LLOAD_LO_HZ, LLOAD_HI_HZ);
        if (err != ESP_OK) { out("E,lload,sweep_start=%s\n", esp_err_to_name(err)); return; }
        s_lload_sweeps = s_lload_errors = 0;
        s_lload = true;
        if (xTaskCreate(lload_task, "cc1101_lload", 3072, NULL, 2, &s_lload_task) != pdPASS) {
            s_lload = false;
            lora_radio_sweep_stop();
            out("E,lload,task\n");
            return;
        }
    } else if (!on) {
        s_lload = false;
    }
    out("I,lload=%d\n", on);
}

static void handle_line(char *buf)
{
    if (!strcmp(buf, "i")) print_info();
    else if (!strcmp(buf, "d")) dump_regs();
    else if (!strcmp(buf, "up")) { bring_up(false); print_info(); }
    else if (!strcmp(buf, "reset")) { bring_up(true); print_info(); }
    else if (!strncmp(buf, "cycle ", 6)) stress(true, atol(buf + 6));
    else if (!strncmp(buf, "rxcyc ", 6)) stress(false, atol(buf + 6));
    else if (!strncmp(buf, "gap ", 4)) { s_gap_us = (uint32_t)atol(buf + 4); s_quiet = false; }
    else if (!strcmp(buf, "quiet")) s_quiet = true;
    else if (!strcmp(buf, "loud")) s_quiet = false;
    else if (!strcmp(buf, "lload on")) lload(true);
    else if (!strcmp(buf, "lload off")) lload(false);
}

static void poll_commands(void)
{
    static char buf[32];
    static size_t n;
    uint8_t c;
    while (usb_serial_jtag_read_bytes(&c, 1, 0) == 1) {
        if (c != '\n' && c != '\r') {
            if (n + 1 < sizeof buf) buf[n++] = (char)c;
            continue;
        }
        buf[n] = '\0';
        if (n) handle_line(buf);
        n = 0;
    }
}

static void check_drift(void)
{
    static uint32_t last;
    if (now_ms() - last < VERIFY_MS) return;
    last = now_ms();
    int16_t at = -1;
    uint8_t marc = 0;
    const cc1101_err_t v = cc1101_verify_profile(&s_bus, &s_profile, &at);
    const cc1101_err_t m = cc1101_read_status(&s_bus, CC1101_MARCSTATE, &marc);
    if (v == CC1101_OK && m == CC1101_OK && (marc & 0x1F) == CC1101_MARC_RX) return;
    s_drifts++;
    out("E,drift,count=%lu,verify=%s,first_mismatch=%d,marc=%s/%02x\n", (unsigned long)s_drifts,
        cc1101_err_name(v), at, cc1101_err_name(m), marc);
    bring_up(false);
    print_info();
}

static void bench_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    out("I,boot,driver=oscilla-c5\n");
    bring_up(true);
    print_info();
    out("H,t_ms,rssi_max,rssi_mean,gdo0_edges\n");
    for (;;) {
        poll_commands();
        if (!s_quiet) check_drift();
        const uint32_t start = now_ms();
        int peak = -200;
        long sum = 0;
        uint32_t count = 0;
        while (now_ms() - start < WINDOW_MS) {
            if (s_quiet) { vTaskDelay(1); continue; }
            if (s_gap_us) esp_rom_delay_us(s_gap_us);
            int16_t dbm = 0;
            if (cc1101_rx_rssi(&s_bus, &dbm) != CC1101_OK) continue;
            if (dbm > peak) peak = dbm;
            sum += dbm;
            count++;
        }
        out("R,%lu,%d,%ld,0\n", (unsigned long)now_ms(), peak, count ? sum / (long)count : 0L);
        vTaskDelay(1);   /* single core: give idle and the radios a tick */
    }
}

esp_err_t cc1101_bench_start(void)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.rx_buffer_size = 256;
    cfg.tx_buffer_size = 4096;
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err != ESP_OK) return err;
    usb_serial_jtag_vfs_use_driver();

    err = cc1101_port_init(&s_bus);
    if (err != ESP_OK) return err;
    s_profile = cc1101_profile_ook_433_async;
    return xTaskCreate(bench_task, "cc1101_bench", 4096, NULL, 2, NULL) == pdPASS ? ESP_OK
                                                                                  : ESP_ERR_NO_MEM;
}

#endif /* CONFIG_OSCILLA_CC1101_BENCH */
