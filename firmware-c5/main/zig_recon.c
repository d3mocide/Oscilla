/* zig_recon.c — D-17 orchestration. Parsing never runs in the radio ISR. */
#include "zig_recon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "ocp.h"
#include "ocp_frame.h"
#include "ocp_parse.h"
#include "radio_arbiter.h"
#include "zig_frame.h"
#include "zig_radio.h"
#include "zig_table.h"

#define ZIG_DEFAULT_CH 11
#define ZIG_DEFAULT_DWELL_MS 400
static zig_table_t s_table;
static SemaphoreHandle_t s_lock;
static bool s_ready;
static volatile bool s_running;
static uint8_t s_ch = ZIG_DEFAULT_CH;
static uint32_t s_dwell = ZIG_DEFAULT_DWELL_MS;
static uint32_t s_last_hop;

static void emit_table(const char *only_pan)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    unsigned pans = 0, nodes = 0;
    for (unsigned i = 0; i < s_table.pan_count; ++i) {
        char pan[5];
        snprintf(pan, sizeof pan, "%04x", s_table.pans[i].pan);
        if (!only_pan || !strcmp(only_pan, pan)) ++pans;
    }
    for (unsigned i = 0; i < s_table.node_count; ++i) {
        char pan[5];
        snprintf(pan, sizeof pan, "%04x", s_table.nodes[i].pan);
        if (!only_pan || !strcmp(only_pan, pan)) ++nodes;
    }
    ocp_emit_begin(OCP_MARK_ZIG, "%s=%u %s=%u %s=%u %s=%u %s=%u %s=%u", OCP_K_COUNT, pans + nodes,
                   OCP_K_PANS, pans, OCP_K_NODES, nodes, OCP_K_DROPPED, s_table.dropped,
                   OCP_K_CH, s_ch, OCP_K_DWELL_MS, s_dwell);
    for (unsigned i = 0; i < s_table.pan_count; ++i) {
        zig_pan_t *p = &s_table.pans[i]; char channels[5] = "";
        char pan[5]; snprintf(pan, sizeof pan, "%04x", p->pan);
        if (only_pan && strcmp(only_pan, pan)) continue;
        snprintf(channels, sizeof channels, "%04x", p->channels);
        ocp_emit_row(OCP_MARK_ZIG, "\"pan\",\"%04x\",\"%s\",\"mac\",\"%s\",\"%u\",\"%d\",\"%u\"",
                     p->pan, p->proto, channels, p->nodes, p->rssi, p->lqi);
    }
    for (unsigned i = 0; i < s_table.node_count; ++i) {
        zig_node_t *n = &s_table.nodes[i]; char pan[5], short_a[5], ext[17] = "";
        snprintf(pan, sizeof pan, "%04x", n->pan); if (only_pan && strcmp(only_pan, pan)) continue;
        if (n->has_short) snprintf(short_a, sizeof short_a, "%04x", n->short_addr); else strcpy(short_a, "");
        if (n->has_ext) for (unsigned j = 0; j < 8; ++j) snprintf(ext + j * 2, sizeof ext - j * 2, "%02x", n->ext[j]);
        ocp_emit_row(OCP_MARK_ZIG, "\"node\",\"%s\",\"%s\",\"%s\",\"unknown\",\"%d\",\"%u\",\"%lu\"",
                     pan, short_a, ext, n->rssi, n->lqi, (unsigned long)n->seen);
    }
    ocp_emit_end(OCP_MARK_ZIG); xSemaphoreGive(s_lock);
}

static void zig_task(void *arg)
{
    (void)arg; zig_rx_t rx;
    for (;;) {
        if (!s_running) { vTaskDelay(pdMS_TO_TICKS(25)); continue; }
        if (zig_radio_next(&rx, 50)) {
            zig_frame_t f;
            if (zig_frame_parse(rx.data, rx.len, &f)) {
                xSemaphoreTake(s_lock, portMAX_DELAY);
                bool fresh = zig_table_upsert(&s_table, f.pan_id, f.proto, rx.ch, f.short_addr, f.has_short, f.ext_addr,
                                              f.has_ext, rx.rssi, rx.lqi, rx.ts_ms);
                xSemaphoreGive(s_lock);
                if (fresh) {
                    ocp_event_record_t event = { .kind = OCP_EVENT_RECORD_ZIG };
                    event.data.zig.pan = f.pan_id;
                    event.data.zig.channel = rx.ch;
                    event.data.zig.rssi = rx.rssi;
                    event.data.zig.lqi = rx.lqi;
                    (void)ocp_event_submit(&event);
                }
            }
        }
        zig_radio_rearm();
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        if (now - s_last_hop >= s_dwell) {
            s_ch = s_ch == 26 ? 11 : s_ch + 1;
            (void)zig_radio_set_channel(s_ch);
            s_last_hop = now;
        }
    }
}

static void zig_teardown(void)
{
    if (!s_running) return;
    s_running = false; zig_radio_stop(); arbiter_release(PHY_OWNER_IEEE802154);
    ocp_emit_compact(OCP_MARK_ZIG, "%s=idle %s=1", OCP_K_STATE, OCP_K_ABORTED);
}

esp_err_t zig_recon_init(void)
{
    s_ready = false;
    s_lock = xSemaphoreCreateMutex(); if (!s_lock) return ESP_ERR_NO_MEM;
    zig_table_clear(&s_table); esp_err_t err = zig_radio_init();
    if (err == ESP_OK && xTaskCreate(zig_task, "zig", 4096, NULL, 5, NULL) != pdPASS) err = ESP_ERR_NO_MEM;
    if (err == ESP_OK) s_ready = true;
    return err;
}
bool zig_recon_ready(void) { return s_ready; }

void zig_cmd_start(int argc, char **argv)
{
    uint8_t ch = ZIG_DEFAULT_CH; uint32_t dwell = ZIG_DEFAULT_DWELL_MS;
    uint32_t parsed = 0;
    if (argc > 1 && !ocp_parse_u32(argv[1], &parsed)) {
        ocp_emit_error(OCP_ERR_BADARG, "ch=11..26 dwell_ms=50..60000"); return;
    }
    if (argc > 1 && (parsed < 11 || parsed > 26)) {
        ocp_emit_error(OCP_ERR_BADARG, "ch=11..26 dwell_ms=50..60000"); return;
    }
    if (argc > 1) ch = (uint8_t)parsed;
    if (argc > 2 && (!ocp_parse_u32(argv[2], &dwell))) {
        ocp_emit_error(OCP_ERR_BADARG, "ch=11..26 dwell_ms=50..60000"); return;
    }
    if (ch < 11 || ch > 26 || dwell < 50 || dwell > 60000) { ocp_emit_error(OCP_ERR_BADARG, "ch=11..26 dwell_ms=50..60000"); return; }
    if (arbiter_acquire(PHY_OWNER_IEEE802154, zig_teardown) != ESP_OK) { ocp_emit_error(OCP_ERR_BUSY, "radio in use"); return; }
    s_ch = ch; s_dwell = dwell; s_last_hop = (uint32_t)(esp_timer_get_time() / 1000);
    if (zig_radio_start(ch) != ESP_OK) { arbiter_release(PHY_OWNER_IEEE802154); ocp_emit_error(OCP_ERR_HWFAULT, "802154 start failed"); return; }
    s_running = true; ocp_emit_compact(OCP_MARK_ZIG, "%s=rx %s=%u %s=%u", OCP_K_STATE, OCP_K_CH, ch, OCP_K_DWELL_MS, dwell);
}
void zig_cmd_status(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint16_t pans = s_table.pan_count, nodes = s_table.node_count;
    xSemaphoreGive(s_lock);
    ocp_emit_compact(OCP_MARK_ZIG, "%s=%s %s=%u %s=%u %s=%u %s=%u", OCP_K_STATE,
                     s_running ? "rx" : "idle", OCP_K_CH, s_ch, OCP_K_DWELL_MS, s_dwell,
                     OCP_K_PANS, pans, OCP_K_NODES, nodes);
}
void zig_cmd_list(void) { emit_table(NULL); }
static bool pan_arg_valid(const char *s)
{
    if (!s || strlen(s) != 4) return false;
    for (unsigned i = 0; i < 4; ++i) if (!strchr("0123456789abcdef", s[i])) return false;
    return true;
}
void zig_cmd_nodes(int argc, char **argv)
{
    if (argc > 1 && !pan_arg_valid(argv[1])) { ocp_emit_error(OCP_ERR_BADARG, "pan=four lowercase hex digits"); return; }
    emit_table(argc > 1 ? argv[1] : NULL);
}
void zig_cmd_clear(void) { xSemaphoreTake(s_lock, portMAX_DELAY); zig_table_clear(&s_table); xSemaphoreGive(s_lock); ocp_emit_compact(OCP_MARK_ZIG, "%s=cleared", OCP_K_STATE); }
