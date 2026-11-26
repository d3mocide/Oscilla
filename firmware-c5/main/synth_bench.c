/*
 * synth_bench.c — see synth_bench.h. Bench builds only; nothing here touches
 * a radio. Events go into the same queue the radio lanes use.
 *
 * SPDX-License-Identifier: MIT
 */

#include "synth_bench.h"
#include "sdkconfig.h"

#if CONFIG_OSCILLA_SYNTH_BENCH

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ocp_frame.h"
#include "synth_plan.h"

#define KEEP_DEFAULT     8u
#define KEEP_MAX         32u
#define KEEP_PERIOD_MS   5000u
#define PROGRESS_MS      10000u
#define SECONDS_MAX      86400u

static synth_plan_t s_plan;
static bool s_running;
static uint32_t s_keep, s_start_ms, s_duration_ms, s_last_keep_ms, s_last_progress_ms;
static uint32_t s_sent, s_keep_sent, s_rejected;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void out(const char *fmt, ...)
{
    char line[200];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n > 0) usb_serial_jtag_write_bytes(line, n < (int)sizeof line ? (size_t)n : sizeof line - 1,
                                           pdMS_TO_TICKS(50));
}

static void emit(synth_lane_t lane, uint32_t index, bool keep)
{
    ocp_event_record_t ev = { .kind = lane == SYNTH_WIFI ? OCP_EVENT_RECORD_NETWORK : OCP_EVENT_RECORD_BLE };
    int8_t rssi = (int8_t)(-40 - (int)(index % 50u));
    if (lane == SYNTH_WIFI) {
        ocp_event_network_t *n = &ev.data.network;
        synth_addr(lane, index, n->bssid);
        n->ssid_len = (uint8_t)snprintf((char *)n->ssid, sizeof n->ssid, "SYNTH-%06lu", (unsigned long)index);
        n->has_ssid = true;
        n->channel = (uint8_t)(1u + index % 11u);
        n->rssi = rssi;
        n->privacy = true;
        n->has_rsn = true;
        n->mfp_capable = (index % 3u) == 0;
        n->interval_ms = 102;
    } else {
        ocp_event_ble_t *b = &ev.data.ble;
        synth_addr(lane, index, b->mac);
        b->name_len = (uint8_t)snprintf((char *)b->name, sizeof b->name, "synth-%06lu", (unsigned long)index);
        b->has_name = true;
        b->rssi = rssi;
    }
    if (!ocp_event_submit(&ev)) s_rejected++;
    else if (keep) s_keep_sent++;
    else s_sent++;
}

static void progress(const char *what)
{
    out("S,%s,t_ms=%lu,wifi=%lu/%lu,ble=%lu/%lu,sent=%lu,keep_sent=%lu,rejected=%lu,"
        "net_drops=%lu,ble_drops=%lu\n",
        what, (unsigned long)(now_ms() - s_start_ms),
        (unsigned long)s_plan.wifi_done, (unsigned long)s_plan.wifi_n,
        (unsigned long)s_plan.ble_done, (unsigned long)s_plan.ble_n,
        (unsigned long)s_sent, (unsigned long)s_keep_sent, (unsigned long)s_rejected,
        (unsigned long)ocp_event_drop_count(OCP_EVENT_RECORD_NETWORK),
        (unsigned long)ocp_event_drop_count(OCP_EVENT_RECORD_BLE));
}

static void start(const char *args)
{
    unsigned long wifi_n = 0, ble_n = 0, seconds = 0, keep = KEEP_DEFAULT;
    const int got = sscanf(args, "%lu %lu %lu %lu", &wifi_n, &ble_n, &seconds, &keep);
    if (got < 3 || seconds == 0 || seconds > SECONDS_MAX || keep > KEEP_MAX ||
        !synth_plan_init(&s_plan, (uint32_t)wifi_n, (uint32_t)ble_n)) {
        out("S,error,usage=synth <wifi_n> <ble_n> <seconds 1..%u> [keep 0..%u]\n", SECONDS_MAX, KEEP_MAX);
        return;
    }
    s_keep = (uint32_t)keep;
    s_duration_ms = (uint32_t)seconds * 1000u;
    s_start_ms = s_last_keep_ms = s_last_progress_ms = now_ms();
    s_sent = s_keep_sent = s_rejected = 0;
    s_running = true;
    progress("start");
}

static void handle_line(const char *buf)
{
    if (!strncmp(buf, "synth ", 6)) start(buf + 6);
    else if (!strcmp(buf, "stop")) { if (s_running) { s_running = false; progress("stopped"); } }
    else if (!strcmp(buf, "status")) progress(s_running ? "running" : "idle");
    else out("S,error,unknown=%s\n", buf);
}

static void poll_commands(void)
{
    static char buf[64];
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

static void tick(void)
{
    const uint32_t now = now_ms();
    const uint32_t elapsed = now - s_start_ms;
    /* Newcomers due by now, spread evenly over the run. */
    const uint64_t total = synth_plan_total(&s_plan);
    const uint64_t due = elapsed >= s_duration_ms ? total : total * elapsed / s_duration_ms;
    uint32_t burst = 0;   /* bound per tick so a slow queue can't starve the loop */
    synth_lane_t lane;
    uint32_t index;
    while ((uint64_t)s_plan.wifi_done + s_plan.ble_done < due && burst++ < 16 &&
           synth_plan_next(&s_plan, &lane, &index)) {
        emit(lane, index, false);
    }
    if (s_keep && now - s_last_keep_ms >= KEEP_PERIOD_MS) {
        s_last_keep_ms = now;
        for (uint32_t i = 0; i < s_keep; i++) {
            if (i < s_plan.wifi_done) emit(SYNTH_WIFI, i, true);
            if (i < s_plan.ble_done) emit(SYNTH_BLE, i, true);
        }
    }
    if (now - s_last_progress_ms >= PROGRESS_MS) {
        s_last_progress_ms = now;
        progress("running");
    }
    if (elapsed >= s_duration_ms && s_plan.wifi_done == s_plan.wifi_n && s_plan.ble_done == s_plan.ble_n) {
        s_running = false;
        progress("done");
    }
}

static void synth_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(500));
    out("S,boot,driver=oscilla-c5-synth\n");
    for (;;) {
        poll_commands();
        if (s_running) tick();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t synth_bench_start(void)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.rx_buffer_size = 256;
    cfg.tx_buffer_size = 2048;
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err != ESP_OK) return err;
    usb_serial_jtag_vfs_use_driver();
    return xTaskCreate(synth_task, "synth_bench", 3072, NULL, 2, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

#endif /* CONFIG_OSCILLA_SYNTH_BENCH */
