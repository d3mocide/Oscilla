/*
 * ocp_frame.c — see ocp_frame.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_frame.h"
#include "ocp_transport.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "ocp.h"
#include "ocp_block.h"
#include "ocp_text.h"
#include "lora_hex.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static SemaphoreHandle_t s_lock;         /* one line at a time */
static SemaphoreHandle_t s_frame_lock;   /* BEGIN..END from one task at a time */
static QueueHandle_t s_event_queue;
static volatile uint32_t s_event_dropped[OCP_EVENT_RECORD_KIND_COUNT];
static ocp_block_t s_frame;

/* A task that never sends END must not wedge the probe: wait, then emit anyway. */
#define FRAME_LOCK_WAIT_MS 2000

static void event_task(void *arg);

esp_err_t ocp_frame_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_frame_lock = xSemaphoreCreateRecursiveMutex();
    s_event_queue = xQueueCreate(OCP_EVENT_QUEUE_DEPTH, sizeof(ocp_event_record_t));
    if (!s_lock || !s_frame_lock || !s_event_queue) return ESP_ERR_NO_MEM;
    return xTaskCreate(event_task, "ocp_evt", 4096, NULL, 3, NULL) == pdPASS
               ? ESP_OK : ESP_ERR_NO_MEM;
}

static bool frame_take(void)
{
    return s_frame_lock &&
           xSemaphoreTakeRecursive(s_frame_lock, pdMS_TO_TICKS(FRAME_LOCK_WAIT_MS)) == pdTRUE;
}

static void frame_give(void)
{
    if (s_frame_lock && xSemaphoreGetMutexHolder(s_frame_lock) == xTaskGetCurrentTaskHandle()) {
        xSemaphoreGiveRecursive(s_frame_lock);
    }
}

static bool write_line(char *line, size_t cap, int n)
{
    if (n < 0 || n + 1 >= (int)cap) return false;
    line[n++] = OCP_LINE_TERM_CHAR;

    if (s_lock && xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool ok = ocp_transport_write(line, (size_t)n);
        xSemaphoreGive(s_lock);
        return ok;
    }
    return false;
}

/* One line, written whole. Over-long output is dropped rather than sent
 * truncated: half a frame is worse than none (OCP-SPEC §1). */
static bool emit_plain(const char *prefix, const char *suffix)
{
    char line[OCP_MAX_LINE_LEN + 2];
    int n = snprintf(line, sizeof line, "%s%s", prefix, suffix ? suffix : "");
    if (n < 0 || n >= (int)sizeof line) return false;
    return write_line(line, sizeof line, n);
}

static bool format_line(char *line, size_t cap, const char *prefix, const char *fmt,
                        va_list ap, const char *suffix, size_t *out_len)
{
    int n = snprintf(line, cap, "%s", prefix);
    if (n < 0 || n >= (int)cap) return false;

    if (fmt && *fmt) {
        int m = vsnprintf(line + n, cap - (size_t)n, fmt, ap);
        if (m < 0 || n + m >= (int)cap) return false;
        n += m;
    }
    if (suffix && *suffix) {
        int m = snprintf(line + n, cap - (size_t)n, "%s", suffix);
        if (m < 0 || n + m >= (int)cap) return false;
        n += m;
    }
    if (n + 1 >= (int)cap) return false;
    line[n++] = OCP_LINE_TERM_CHAR;
    *out_len = (size_t)n;
    return true;
}

static bool emit(const char *prefix, const char *fmt, va_list ap, const char *suffix)
{
    char line[OCP_MAX_LINE_LEN + 2];
    size_t len;
    if (!format_line(line, sizeof line, prefix, fmt, ap, suffix, &len)) return false;
    return write_line(line, sizeof line, (int)(len - 1));
}

#define FRAME_MAX_BYTES ((size_t)OCP_MAX_FRAME_ROWS * (OCP_MAX_LINE_LEN + 2u))

static bool frame_append(const char *line, size_t len)
{
    return ocp_block_append(&s_frame, line, len, FRAME_MAX_BYTES);
}

static bool frame_append_formatted(const char *prefix, const char *fmt, va_list ap,
                                   const char *suffix)
{
    char line[OCP_MAX_LINE_LEN + 2];
    size_t len;
    if (!format_line(line, sizeof line, prefix, fmt, ap, suffix, &len)) {
        s_frame.failed = true;
        return false;
    }
    return frame_append(line, len);
}

static bool frame_writer(const char *bytes, size_t len, void *arg)
{
    (void)arg;
    if (!s_lock || xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) return false;
    bool ok = ocp_transport_write(bytes, len);
    xSemaphoreGive(s_lock);
    return ok;
}

static bool frame_commit(void)
{
    return ocp_block_commit(&s_frame, frame_writer, NULL);
}

static void frame_discard(void)
{
    ocp_block_discard(&s_frame);
}

static const char *event_kind_name(ocp_event_record_kind_t kind)
{
    switch (kind) {
        case OCP_EVENT_RECORD_NETWORK: return OCP_EVT_KIND_NETWORK;
        case OCP_EVENT_RECORD_CLIENT:  return OCP_EVT_KIND_CLIENT;
        case OCP_EVENT_RECORD_PROBE:   return OCP_EVT_KIND_PROBE;
        case OCP_EVENT_RECORD_DEAUTH:  return OCP_EVT_KIND_DEAUTH;
        case OCP_EVENT_RECORD_BLE:     return OCP_EVT_KIND_BLE;
        case OCP_EVENT_RECORD_AIRTAG:  return OCP_EVT_KIND_AIRTAG;
        case OCP_EVENT_RECORD_SNIFF:   return OCP_EVT_KIND_SNIFF;
        case OCP_EVENT_RECORD_CHAN:    return OCP_EVT_KIND_CHAN;
        case OCP_EVENT_RECORD_ZIG:     return OCP_EVT_KIND_ZIG;
        case OCP_EVENT_RECORD_LORA:    return OCP_EVT_KIND_LORA;
        case OCP_EVENT_RECORD_KIND_COUNT: break;
    }
    return "unknown";
}

static void emit_event_fields(const char *kind, const char *fmt, ...)
{
    char prefix[64];
    int n = snprintf(prefix, sizeof prefix, "%s %s=%s ", OCP_MARK_EVT, OCP_K_KIND, kind);
    if (n < 0 || n >= (int)sizeof prefix) return;
    va_list ap;
    va_start(ap, fmt);
    emit(prefix, fmt, ap, "");
    va_end(ap);
}

static void emit_event_record(const ocp_event_record_t *event)
{
    switch (event->kind) {
        case OCP_EVENT_RECORD_NETWORK: {
            const ocp_event_network_t *r = &event->data.network;
            char ssid[4 * 32 + 3];
            ocp_escape_field(r->ssid, r->has_ssid ? r->ssid_len : 0, ssid, sizeof ssid);
            emit_event_fields(event_kind_name(event->kind),
                              "%s=%02x:%02x:%02x:%02x:%02x:%02x %s=%s %s=%u %s=%s %s=%d %s=%d %s=%d %s=%d %s=%d %s=%u",
                              OCP_K_BSSID, r->bssid[0], r->bssid[1], r->bssid[2], r->bssid[3], r->bssid[4], r->bssid[5],
                              OCP_K_SSID, ssid, OCP_K_CH, (unsigned)r->channel,
                              OCP_K_BAND, r->channel <= 14 ? OCP_BAND_LABEL_24 : OCP_BAND_LABEL_5,
                              OCP_K_RSSI, r->rssi, OCP_K_PRIVACY, r->privacy, OCP_K_RSN, r->has_rsn,
                              OCP_K_MFP_CAPABLE, r->mfp_capable, OCP_K_MFP_REQUIRED, r->mfp_required,
                              OCP_K_INTERVAL_MS, (unsigned)r->interval_ms);
            break;
        }
        case OCP_EVENT_RECORD_CLIENT: {
            const ocp_event_client_t *r = &event->data.client;
            emit_event_fields(event_kind_name(event->kind),
                              "%s=%02x:%02x:%02x:%02x:%02x:%02x %s=%02x:%02x:%02x:%02x:%02x:%02x %s=%u %s=%d",
                              OCP_K_BSSID, r->bssid[0], r->bssid[1], r->bssid[2], r->bssid[3], r->bssid[4], r->bssid[5],
                              OCP_K_MAC, r->mac[0], r->mac[1], r->mac[2], r->mac[3], r->mac[4], r->mac[5],
                              OCP_K_CH, r->channel, OCP_K_RSSI, r->rssi);
            break;
        }
        case OCP_EVENT_RECORD_PROBE: {
            const ocp_event_probe_t *r = &event->data.probe;
            char ssid[4 * 32 + 3];
            ocp_escape_field(r->ssid, r->ssid_len, ssid, sizeof ssid);
            emit_event_fields(event_kind_name(event->kind),
                              "%s=%02x:%02x:%02x:%02x:%02x:%02x %s=%s %s=%d",
                              OCP_K_MAC, r->mac[0], r->mac[1], r->mac[2], r->mac[3], r->mac[4], r->mac[5],
                              OCP_K_SSID, ssid, OCP_K_RSSI, r->rssi);
            break;
        }
        case OCP_EVENT_RECORD_DEAUTH: {
            const ocp_event_deauth_t *r = &event->data.deauth;
            emit_event_fields(event_kind_name(event->kind),
                              "%s=%02x:%02x:%02x:%02x:%02x:%02x %s=%02x:%02x:%02x:%02x:%02x:%02x %s=%u %s=%u %s=%d %s=%u %s=%u",
                              OCP_K_BSSID, r->bssid[0], r->bssid[1], r->bssid[2], r->bssid[3], r->bssid[4], r->bssid[5],
                              OCP_K_MAC, r->mac[0], r->mac[1], r->mac[2], r->mac[3], r->mac[4], r->mac[5],
                              OCP_K_REASON, r->reason, OCP_K_DISASSOC, r->is_disassoc ? 1u : 0u,
                              OCP_K_RSSI, r->rssi, OCP_K_CH, r->channel, OCP_K_COUNT, r->count);
            break;
        }
        case OCP_EVENT_RECORD_BLE: {
            const ocp_event_ble_t *r = &event->data.ble;
            char name[4 * 32 + 3], mfr[8];
            ocp_escape_field(r->name, r->has_name ? r->name_len : 0, name, sizeof name);
            if (r->has_mfr) snprintf(mfr, sizeof mfr, "\"%04x\"", r->mfr_company_id);
            else snprintf(mfr, sizeof mfr, "\"\"");
            emit_event_fields(event_kind_name(event->kind),
                              "%s=%02x:%02x:%02x:%02x:%02x:%02x %s=%s %s=%s %s=%s %s=%d",
                              OCP_K_MAC, r->mac[0], r->mac[1], r->mac[2], r->mac[3], r->mac[4], r->mac[5],
                              OCP_K_NAME, name, OCP_K_MFR, mfr,
                              OCP_K_TRACKER, r->is_tracker ? "\"" OCP_EVT_KIND_AIRTAG "\"" : "\"\"",
                              OCP_K_RSSI, r->rssi);
            break;
        }
        case OCP_EVENT_RECORD_AIRTAG: {
            const ocp_event_airtag_t *r = &event->data.airtag;
            emit_event_fields(event_kind_name(event->kind),
                              "%s=%02x:%02x:%02x:%02x:%02x:%02x %s=%d %s=%lu",
                              OCP_K_MAC, r->mac[0], r->mac[1], r->mac[2], r->mac[3], r->mac[4], r->mac[5],
                              OCP_K_RSSI, r->rssi, OCP_K_COUNT, (unsigned long)r->count);
            break;
        }
        case OCP_EVENT_RECORD_SNIFF: {
            const ocp_event_sniff_t *r = &event->data.sniff;
            emit_event_fields(event_kind_name(event->kind), "%s=%u %s=%u",
                              OCP_K_PKTS, (unsigned)r->packets, OCP_K_CH, r->channel);
            break;
        }
        case OCP_EVENT_RECORD_CHAN: {
            const ocp_event_chan_t *r = &event->data.chan;
            emit_event_fields(event_kind_name(event->kind), "%s=%u %s=%u",
                              OCP_K_CH, r->channel, OCP_K_PKTS, (unsigned)r->packets);
            break;
        }
        case OCP_EVENT_RECORD_ZIG: {
            const ocp_event_zig_t *r = &event->data.zig;
            emit_event_fields(event_kind_name(event->kind), "pan=%04x ch=%u rssi=%d lqi=%u",
                              r->pan, r->channel, r->rssi, r->lqi);
            break;
        }
        case OCP_EVENT_RECORD_LORA: {
            const ocp_event_lora_t *r = &event->data.lora;
            char hex[255 * 2 + 1];
            if (!lora_bytes_to_hex(r->payload, r->len, hex, sizeof hex, NULL)) return;
            emit_event_fields(event_kind_name(event->kind), "%s=%d %s=%.1f %s=%u %s=%s",
                              OCP_K_RSSI, r->rssi_dbm, OCP_K_SNR, (double)r->snr_db,
                              OCP_K_LEN, (unsigned)r->len, OCP_K_HEX, hex);
            break;
        }
        case OCP_EVENT_RECORD_KIND_COUNT:
            break;
    }
}

static void event_task(void *arg)
{
    (void)arg;
    ocp_event_record_t event;
    for (;;) {
        if (xQueueReceive(s_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            emit_event_record(&event);
        }
    }
}

bool ocp_event_submit(const ocp_event_record_t *event)
{
    if (!event || event->kind >= OCP_EVENT_RECORD_KIND_COUNT || !s_event_queue) return false;
    if (xQueueSend(s_event_queue, event, 0) != pdTRUE) {
        ++s_event_dropped[event->kind];
        return false;
    }
    return true;
}

uint32_t ocp_event_drop_count(ocp_event_record_kind_t kind)
{
    return kind < OCP_EVENT_RECORD_KIND_COUNT ? s_event_dropped[kind] : 0;
}

void ocp_emit_compact(const char *tag, const char *fmt, ...)
{
    if (!frame_take()) return;
    va_list ap;
    va_start(ap, fmt);
    char prefix[OCP_MAX_VERB_LEN + 2];
    snprintf(prefix, sizeof prefix, "%s ", tag);
    (void)emit(prefix, fmt, ap, " " OCP_KW_END);
    frame_give();
    va_end(ap);
}

bool ocp_emit_begin(const char *tag, const char *fmt, ...)
{
    if (!frame_take()) return false;
    ocp_block_begin(&s_frame);
    va_list ap;
    va_start(ap, fmt);
    char prefix[OCP_MAX_VERB_LEN + 8];
    snprintf(prefix, sizeof prefix, "%s %s", tag, OCP_KW_BEGIN " ");
    bool ok = frame_append_formatted(prefix, fmt, ap, "");
    va_end(ap);
    if (!ok) {
        frame_discard();
        frame_give();
    }
    return ok;
}

bool ocp_emit_row(const char *tag, const char *fmt, ...)
{
    if (!s_frame.active || s_frame.failed) return false;
    va_list ap;
    va_start(ap, fmt);
    char prefix[OCP_MAX_VERB_LEN + 2];
    snprintf(prefix, sizeof prefix, "%s ", tag);
    bool ok = frame_append_formatted(prefix, fmt, ap, "");
    va_end(ap);
    return ok;
}

bool ocp_emit_end(const char *tag)
{
    if (!s_frame.active) return false;
    char prefix[OCP_MAX_VERB_LEN + 8];
    snprintf(prefix, sizeof prefix, "%s %s", tag, OCP_KW_END);
    char line[OCP_MAX_LINE_LEN + 2];
    int n = snprintf(line, sizeof line, "%s\n", prefix);
    bool ok = n >= 0 && n < (int)sizeof line && frame_append(line, (size_t)n);
    if (ok) ok = frame_commit();
    frame_discard();
    frame_give();
    return ok;
}

void ocp_emit_error(const char *code, const char *msg)
{
    char escaped[256];
    ocp_escape_field((const uint8_t *)msg, strlen(msg), escaped, sizeof escaped);

    char prefix[OCP_MAX_LINE_LEN];
    snprintf(prefix, sizeof prefix, "%s %s=%s %s=%s",
             OCP_MARK_ERR, OCP_K_CODE, code, OCP_K_MSG, escaped);
    if (!frame_take()) return;
    (void)emit_plain(prefix, "");
    frame_give();
}

void ocp_emit_literal(const char *text)
{
    if (!frame_take()) return;
    (void)emit_plain(text, "");
    frame_give();
}
