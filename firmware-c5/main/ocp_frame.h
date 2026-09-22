/*
 * ocp_frame.h — marker-frame emission (OCP-SPEC.md §3).
 *
 * Every emitter writes whole lines under a lock. A block frame also holds a
 * frame lock from BEGIN to END, so another task's reply cannot land between
 * its rows; [EVT] lines skip that lock, as the spec allows (OCP-SPEC §5.1).
 * Always pair ocp_emit_begin() with ocp_emit_end().
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef OSCILLA_OCP_FRAME_H
#define OSCILLA_OCP_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* Fixed-size records cross the receive callback -> emitter-task boundary.
 * Producers never format text or touch the transport (OCP-SPEC §5.1). */
typedef enum {
    OCP_EVENT_RECORD_NETWORK,
    OCP_EVENT_RECORD_CLIENT,
    OCP_EVENT_RECORD_PROBE,
    OCP_EVENT_RECORD_DEAUTH,
    OCP_EVENT_RECORD_BLE,
    OCP_EVENT_RECORD_AIRTAG,
    OCP_EVENT_RECORD_SNIFF,
    OCP_EVENT_RECORD_CHAN,
    OCP_EVENT_RECORD_ZIG,
    OCP_EVENT_RECORD_LORA,
    OCP_EVENT_RECORD_KIND_COUNT,
} ocp_event_record_kind_t;

typedef struct {
    uint8_t bssid[6], ssid[32], ssid_len, channel;
    int8_t rssi;
    bool has_ssid, privacy, has_rsn, mfp_capable, mfp_required;
    uint16_t interval_ms;
} ocp_event_network_t;
typedef struct { uint8_t bssid[6], mac[6], channel; int8_t rssi; } ocp_event_client_t;
typedef struct { uint8_t mac[6], ssid[32], ssid_len; int8_t rssi; } ocp_event_probe_t;
typedef struct {
    uint8_t bssid[6], mac[6], channel;
    uint16_t reason;
    bool is_disassoc;
    int8_t rssi;
    uint32_t count;
} ocp_event_deauth_t;
typedef struct {
    uint8_t mac[6], name[32], name_len;
    uint16_t mfr_company_id;
    bool has_name, has_mfr, is_tracker;
    int8_t rssi;
    uint32_t count;
} ocp_event_ble_t;
typedef struct { uint8_t mac[6]; int8_t rssi; uint32_t count; } ocp_event_airtag_t;
typedef struct { uint8_t channel; uint32_t packets; } ocp_event_sniff_t;
typedef struct { uint8_t channel; uint32_t packets; } ocp_event_chan_t;
typedef struct { uint16_t pan; uint8_t channel, lqi; int8_t rssi; } ocp_event_zig_t;
typedef struct { uint8_t len, payload[255]; int16_t rssi_dbm; float snr_db; } ocp_event_lora_t;

typedef struct {
    ocp_event_record_kind_t kind;
    union {
        ocp_event_network_t network;
        ocp_event_client_t client;
        ocp_event_probe_t probe;
        ocp_event_deauth_t deauth;
        ocp_event_ble_t ble;
        ocp_event_airtag_t airtag;
        ocp_event_sniff_t sniff;
        ocp_event_chan_t chan;
        ocp_event_zig_t zig;
        ocp_event_lora_t lora;
    } data;
} ocp_event_record_t;

esp_err_t ocp_frame_init(void);

/* [TAG] k=v ... END on one line. */
void ocp_emit_compact(const char *tag, const char *fmt, ...);

/* [TAG] BEGIN k=v ... / [TAG] <row> / [TAG] END */
bool ocp_emit_begin(const char *tag, const char *fmt, ...);
bool ocp_emit_row(const char *tag, const char *fmt, ...);
bool ocp_emit_end(const char *tag);

/* Nonblocking producer boundary for [EVT] records. */
bool ocp_event_submit(const ocp_event_record_t *event);
uint32_t ocp_event_drop_count(ocp_event_record_kind_t kind);

/* [ERR] code=<code> msg="..." — msg is escaped, never format-injected. */
void ocp_emit_error(const char *code, const char *msg);

/* Bare replies with no marker; `pong` is the only one (OCP-SPEC §3.4). */
void ocp_emit_literal(const char *text);

#endif /* OSCILLA_OCP_FRAME_H */
