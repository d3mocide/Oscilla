/*
 * ocp.h — Oscilla Control Protocol, v1.
 *
 * THE contract between the deck (oscilla-cp, ESP32-S3) and the probe
 * (oscilla-c5, ESP32-C5). Both firmwares add `../protocol` to their include
 * path and #include this file, so a rename here breaks both builds until
 * fixed — the contract cannot silently drift.
 *
 * Human-readable companion: protocol/OCP-SPEC.md (normative for wire
 * behaviour; this header is normative for the literals).
 * Architecture: DESIGN.md §5.  Hardware: Research/c5-backpack-design.md Rev D.
 *
 * This header is data only: string literals, limits, and one X-macro verb
 * table. It allocates nothing, includes nothing, and compiles as C99 and
 * C++11 alike.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Will Shields
 */

#ifndef OSCILLA_OCP_H
#define OSCILLA_OCP_H

/* ------------------------------------------------------------------------
 * §8 / D-8 — receive-only, enforced at build time.
 *
 * Oscilla v1 compiles no transmit verb into any build, so no reachable
 * firmware code path to transmission exists on any radio. These flags are
 * the names such a future effort would use; tripping this #error means
 * someone is reintroducing transmit without reopening D-8/D-13 first.
 * (The SX1262 is TX-capable silicon — the guarantee is about code paths.)
 * ------------------------------------------------------------------------ */
#if defined(OSCILLA_WIFI_TX) || defined(OSCILLA_BLE_TX) || \
    defined(OSCILLA_154_TX) || defined(OSCILLA_LORA_TX) || defined(OSCILLA_TX)
#error "Oscilla v1 is receive-only (DESIGN.md §8, docs/DECISIONS.md D-8). No transmit build flag is recognised."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------
 * Protocol identity and transport
 * ------------------------------------------------------------------------ */

/* Single integer. Bumped only on a breaking change to framing or handshake;
 * new verbs and new [EVT] kinds are additive and do NOT bump it (§5.7). */
#define OCP_PROTO_VERSION       1
#define OCP_PROTO_VERSION_STR   "1"

#define OCP_FW_NAME_PROBE       "oscilla-c5"
#define OCP_FW_NAME_DECK        "oscilla-cp"

/* Fixed at boot so a plain terminal always works (§5.1). */
#define OCP_BAUD_DEFAULT        115200
#define OCP_DATA_BITS           8
#define OCP_PARITY              'N'
#define OCP_STOP_BITS           1

/* Commands are LF-terminated; the probe also tolerates CRLF. */
#define OCP_LINE_TERM           "\n"
#define OCP_LINE_TERM_CHAR      '\n'

/* ------------------------------------------------------------------------
 * Bounded messages (§5.1, Rev D §10) — both ends cap and drop, never block.
 * ------------------------------------------------------------------------ */
#define OCP_MAX_LINE_LEN        512  /* bytes, excluding the terminator */
#define OCP_MAX_ARGV            10   /* verb + up to 9 arguments         */
#define OCP_MAX_VERB_LEN        32
#define OCP_MAX_SSID_LEN        32   /* 802.11 limit; +1 for NUL         */
#define OCP_MAX_BSSID_STR_LEN   17   /* "AA:BB:CC:DD:EE:FF"              */
#define OCP_EVENT_QUEUE_DEPTH   32   /* drop-oldest past this            */

/* ------------------------------------------------------------------------
 * Framing keywords (§5.2)
 *
 *   block frame:    [TAG] BEGIN [k=v ...]
 *                   [TAG] <row>
 *                   [TAG] END
 *   compact frame:  [TAG] [k=v ...] END        (single line)
 *   bare line:      [EVT] k=v ...  /  [ERR] code=... msg="..."
 *
 * Any line that is neither a known marker nor an expected row is ignored —
 * that is how the deck rides out C5 ROM boot chatter (§4.2). Unknown markers
 * and unknown k=v keys are ignored, never fatal.
 * ------------------------------------------------------------------------ */
#define OCP_KW_BEGIN            "BEGIN"
#define OCP_KW_END              "END"

/* Marker registry (Appendix B). Brackets are part of the literal. */
#define OCP_MARK_HELLO          "[HELLO]"
#define OCP_MARK_VER            "[VER]"
#define OCP_MARK_STATUS         "[STATUS]"
#define OCP_MARK_STOP           "[STOP]"
#define OCP_MARK_CFG            "[CFG]"
#define OCP_MARK_SCAN           "[SCAN]"
#define OCP_MARK_INSPECT        "[INSPECT]"
#define OCP_MARK_SNIFF          "[SNIFF]"
#define OCP_MARK_CLIENTS        "[CLIENTS]"
#define OCP_MARK_PROBES         "[PROBES]"
#define OCP_MARK_CHAN           "[CHAN]"
#define OCP_MARK_BLE            "[BLE]"
#define OCP_MARK_ZIG            "[ZIG]"
#define OCP_MARK_LORA           "[LORA]"
#define OCP_MARK_EVT            "[EVT]"
#define OCP_MARK_ERR            "[ERR]"
#define OCP_MARK_FT             "[FT]"   /* reserved, unimplemented (§5.6, D-2) */

/* The one non-marker reply, kept because a human types `ping` at a terminal
 * and wants to read `pong` back. Deck parsers must recognise it explicitly. */
#define OCP_REPLY_PONG          "pong"

/* ------------------------------------------------------------------------
 * Capabilities (§5.3)
 *
 * Every cap names a RECEIVE capability. There is no transmit cap because
 * there is no transmit verb (§8).
 * ------------------------------------------------------------------------ */
#define OCP_CAP_WIFI24          "wifi24"
#define OCP_CAP_WIFI5           "wifi5"
#define OCP_CAP_BLE             "ble"
#define OCP_CAP_IEEE802154      "ieee802154"
#define OCP_CAP_LORA_RX         "lora_rx"

#define OCP_CAP_SEP             ","   /* caps=wifi24,wifi5,ble,... */

/* Verb gating works on capability *classes*, not individual cap strings:
 * a Wi-Fi verb is available if either wifi24 or wifi5 is advertised. */
typedef enum {
    OCP_CC_NONE = 0,   /* always available — system verbs */
    OCP_CC_WIFI,       /* satisfied by OCP_CAP_WIFI24 or OCP_CAP_WIFI5 */
    OCP_CC_BLE,
    OCP_CC_IEEE802154,
    OCP_CC_LORA_RX
} ocp_cap_class_t;

/* ------------------------------------------------------------------------
 * Verbs (Appendix A). All receive-only — the command table IS the attack
 * surface (§8), so this list is the guarantee.
 * ------------------------------------------------------------------------ */

/* System */
#define OCP_V_HELLO             "hello"
#define OCP_V_PING              "ping"
#define OCP_V_VERSION           "version"
#define OCP_V_STATUS            "status"
#define OCP_V_STOP              "stop"
#define OCP_V_REBOOT            "reboot"

/* Wi-Fi */
#define OCP_V_SCAN_NETWORKS     "scan_networks"
#define OCP_V_SHOW_SCAN_RESULTS "show_scan_results"
#define OCP_V_INSPECT_NETWORK   "inspect_network"
#define OCP_V_START_SNIFFER     "start_sniffer"
#define OCP_V_SHOW_CLIENTS      "show_clients"
#define OCP_V_SHOW_PROBES       "show_probes"
#define OCP_V_CHANNEL_VIEW      "channel_view"
#define OCP_V_PACKET_MONITOR    "packet_monitor"
#define OCP_V_DEAUTH_DETECTOR   "deauth_detector"
#define OCP_V_SET_BAND          "set_band"
#define OCP_V_SET_CHANNELS      "set_channels"

/* BLE */
#define OCP_V_SCAN_BT           "scan_bt"
#define OCP_V_SCAN_AIRTAG       "scan_airtag"
#define OCP_V_START_ANTISURV    "start_antisurveillance"

/* 802.15.4 */
#define OCP_V_START_ZIG_RECON   "start_zig_recon"
#define OCP_V_ZIG_STATUS        "zig_recon_status"
#define OCP_V_ZIG_LIST          "zig_recon_list"
#define OCP_V_ZIG_NODES         "zig_recon_nodes"
#define OCP_V_ZIG_CLEAR         "zig_recon_clear"

/* LoRa — receive only */
#define OCP_V_LORA_CONFIG       "lora_config"
#define OCP_V_LORA_LISTEN       "lora_listen"
#define OCP_V_LORA_STATUS       "lora_status"

/* Mixed */
#define OCP_V_START_WARDRIVE    "start_wardrive"

/*
 * The command table, as an X-macro over the literals above so there is
 * exactly one definition of each verb string in the system.
 *
 *   X(id, verb, cap_class, min_args, max_args, reply_marker)
 *
 * The probe builds its dispatch table from this; tools derive help and
 * argument checking from it; the P3 exit gate greps it for TX verbs and
 * must find none.
 */
#define OCP_VERB_TABLE(X)                                                                          \
    /*   id              verb                     cap class          min max  reply            */  \
    X(HELLO,             OCP_V_HELLO,             OCP_CC_NONE,        0,  0,  OCP_MARK_HELLO)      \
    X(PING,              OCP_V_PING,              OCP_CC_NONE,        0,  0,  OCP_REPLY_PONG)      \
    X(VERSION,           OCP_V_VERSION,           OCP_CC_NONE,        0,  0,  OCP_MARK_VER)        \
    X(STATUS,            OCP_V_STATUS,            OCP_CC_NONE,        0,  0,  OCP_MARK_STATUS)     \
    X(STOP,              OCP_V_STOP,              OCP_CC_NONE,        0,  0,  OCP_MARK_STOP)       \
    X(REBOOT,            OCP_V_REBOOT,            OCP_CC_NONE,        0,  0,  "")                  \
    X(SCAN_NETWORKS,     OCP_V_SCAN_NETWORKS,     OCP_CC_WIFI,        0,  0,  OCP_MARK_SCAN)       \
    X(SHOW_SCAN_RESULTS, OCP_V_SHOW_SCAN_RESULTS, OCP_CC_WIFI,        0,  0,  OCP_MARK_SCAN)       \
    X(INSPECT_NETWORK,   OCP_V_INSPECT_NETWORK,   OCP_CC_WIFI,        1,  1,  OCP_MARK_INSPECT)    \
    X(START_SNIFFER,     OCP_V_START_SNIFFER,     OCP_CC_WIFI,        0,  0,  OCP_MARK_SNIFF)      \
    X(SHOW_CLIENTS,      OCP_V_SHOW_CLIENTS,      OCP_CC_WIFI,        0,  0,  OCP_MARK_CLIENTS)    \
    X(SHOW_PROBES,       OCP_V_SHOW_PROBES,       OCP_CC_WIFI,        0,  0,  OCP_MARK_PROBES)     \
    X(CHANNEL_VIEW,      OCP_V_CHANNEL_VIEW,      OCP_CC_WIFI,        0,  0,  OCP_MARK_CHAN)       \
    X(PACKET_MONITOR,    OCP_V_PACKET_MONITOR,    OCP_CC_WIFI,        1,  1,  OCP_MARK_CFG)        \
    X(DEAUTH_DETECTOR,   OCP_V_DEAUTH_DETECTOR,   OCP_CC_WIFI,        0,  0,  OCP_MARK_CFG)        \
    X(SET_BAND,          OCP_V_SET_BAND,          OCP_CC_WIFI,        1,  1,  OCP_MARK_CFG)        \
    X(SET_CHANNELS,      OCP_V_SET_CHANNELS,      OCP_CC_WIFI,        1,  1,  OCP_MARK_CFG)        \
    X(SCAN_BT,           OCP_V_SCAN_BT,           OCP_CC_BLE,         0,  1,  OCP_MARK_BLE)        \
    X(SCAN_AIRTAG,       OCP_V_SCAN_AIRTAG,       OCP_CC_BLE,         0,  0,  OCP_MARK_CFG)        \
    X(START_ANTISURV,    OCP_V_START_ANTISURV,    OCP_CC_BLE,         0,  0,  OCP_MARK_CFG)        \
    X(START_ZIG_RECON,   OCP_V_START_ZIG_RECON,   OCP_CC_IEEE802154,  0,  2,  OCP_MARK_ZIG)        \
    X(ZIG_STATUS,        OCP_V_ZIG_STATUS,        OCP_CC_IEEE802154,  0,  0,  OCP_MARK_ZIG)        \
    X(ZIG_LIST,          OCP_V_ZIG_LIST,          OCP_CC_IEEE802154,  0,  0,  OCP_MARK_ZIG)        \
    X(ZIG_NODES,         OCP_V_ZIG_NODES,         OCP_CC_IEEE802154,  0,  1,  OCP_MARK_ZIG)        \
    X(ZIG_CLEAR,         OCP_V_ZIG_CLEAR,         OCP_CC_IEEE802154,  0,  0,  OCP_MARK_ZIG)        \
    X(LORA_CONFIG,       OCP_V_LORA_CONFIG,       OCP_CC_LORA_RX,     4,  4,  OCP_MARK_CFG)        \
    X(LORA_LISTEN,       OCP_V_LORA_LISTEN,       OCP_CC_LORA_RX,     0,  0,  OCP_MARK_LORA)       \
    X(LORA_STATUS,       OCP_V_LORA_STATUS,       OCP_CC_LORA_RX,     0,  0,  OCP_MARK_LORA)       \
    X(START_WARDRIVE,    OCP_V_START_WARDRIVE,    OCP_CC_WIFI,        0,  4,  OCP_MARK_CFG)

/* ------------------------------------------------------------------------
 * Errors (§5.5) — [ERR] code=<code> [k=v ...] msg="..."
 * ------------------------------------------------------------------------ */
#define OCP_ERR_BUSY            "busy"      /* PHY lane held by another owner; owner= names it */
#define OCP_ERR_BADARG          "badarg"    /* wrong arity, or an argument out of range        */
#define OCP_ERR_BUDGET          "budget"    /* refused by the power interlock (§6.2)           */
#define OCP_ERR_HWFAULT         "hwfault"   /* peripheral did not respond (e.g. SX1262 BUSY)   */
#define OCP_ERR_UNKNOWN         "unknown"   /* verb not in this build's command table          */
#define OCP_ERR_NOCAP           "nocap"     /* verb known, capability absent in this build     */
#define OCP_ERR_INTERNAL        "internal"  /* allocation/queue failure — a probe-side bug     */

/* ------------------------------------------------------------------------
 * Event kinds (§5.4) — [EVT] kind=<kind> ...
 * Additive: an unknown kind is ignored, never fatal.
 * ------------------------------------------------------------------------ */
#define OCP_EVT_KIND_SNIFF      "sniff"
#define OCP_EVT_KIND_FOLLOWER   "follower"
#define OCP_EVT_KIND_AIRTAG     "airtag"
#define OCP_EVT_KIND_CHAN       "chan"
#define OCP_EVT_KIND_LORA       "lora"
#define OCP_EVT_KIND_CLIENT     "client"
#define OCP_EVT_KIND_PROBE      "probe"
#define OCP_EVT_KIND_DEAUTH     "deauth"
#define OCP_EVT_KIND_WARDRIVE   "wardrive"

/* ------------------------------------------------------------------------
 * Well-known keys and row shapes
 * ------------------------------------------------------------------------ */
#define OCP_K_PROTO             "proto"
#define OCP_K_FW                "fw"
#define OCP_K_VER               "ver"
#define OCP_K_CAPS              "caps"
#define OCP_K_KIND              "kind"
#define OCP_K_CODE              "code"
#define OCP_K_MSG               "msg"
#define OCP_K_OWNER             "owner"
#define OCP_K_COUNT             "n"
#define OCP_K_RUNNING           "running"
#define OCP_K_UPTIME_MS         "uptime_ms"
#define OCP_K_TS_MS             "ts_ms"    /* monotonic probe timestamp on observations (§9.1) */

/* [SCAN] rows, emitted after the BEGIN line. Escaping: OCP-SPEC.md §6. */
#define OCP_SCAN_CSV_HEADER     "\"idx\",\"ssid\",\"bssid\",\"ch\",\"auth\",\"rssi\",\"band\""
#define OCP_SCAN_CSV_FIELDS     7

/* set_band arguments */
#define OCP_BAND_24             "24"
#define OCP_BAND_5              "5"
#define OCP_BAND_AUTO           "auto"

/* PHY-lane owner names, as reported by [STATUS] and [ERR] owner= */
#define OCP_OWNER_NONE          "none"
#define OCP_OWNER_WIFI          "wifi"
#define OCP_OWNER_BLE           "ble"
#define OCP_OWNER_IEEE802154    "ieee802154"

/* ------------------------------------------------------------------------
 * Small pure helpers. Header-only, no allocation.
 * ------------------------------------------------------------------------ */

/* Name of a capability class, for error text and tooling. */
static inline const char *ocp_cap_class_name(ocp_cap_class_t cc)
{
    switch (cc) {
        case OCP_CC_WIFI:        return "wifi";
        case OCP_CC_BLE:         return OCP_CAP_BLE;
        case OCP_CC_IEEE802154:  return OCP_CAP_IEEE802154;
        case OCP_CC_LORA_RX:     return OCP_CAP_LORA_RX;
        case OCP_CC_NONE:        break;
    }
    return "";
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* OSCILLA_OCP_H */
