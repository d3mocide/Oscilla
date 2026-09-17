/*
 * ocp.h — Oscilla Control Protocol v1. The contract both firmwares compile
 * against. Literals live here; wire behaviour is protocol/OCP-SPEC.md.
 *
 * C99 and C++11 clean. Data only: no includes, no allocation.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 d3FRAG Networks
 */

#ifndef OSCILLA_OCP_H
#define OSCILLA_OCP_H

/* Receive-only (DESIGN §8, D-8). Tripping this means transmit is being
 * reintroduced without reopening the decision. */
#if defined(OSCILLA_WIFI_TX) || defined(OSCILLA_BLE_TX) || \
    defined(OSCILLA_154_TX) || defined(OSCILLA_LORA_TX) || defined(OSCILLA_TX)
#error "Oscilla v1 is receive-only (DESIGN.md §8, docs/DECISIONS.md D-8). No transmit build flag is recognised."
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* --- Identity and transport ---------------------------------------------- */

/* Bumped only on a breaking framing/handshake change (OCP-SPEC §1). */
#define OCP_PROTO_VERSION       1
#define OCP_PROTO_VERSION_STR   "1"

#define OCP_FW_NAME_PROBE       "oscilla-c5"
#define OCP_FW_NAME_DECK        "oscilla-cp"

#define OCP_BAUD_DEFAULT        115200
#define OCP_DATA_BITS           8
#define OCP_PARITY              'N'
#define OCP_STOP_BITS           1

/* Commands are LF-terminated; the probe also accepts CRLF. */
#define OCP_LINE_TERM           "\n"
#define OCP_LINE_TERM_CHAR      '\n'

/* --- Limits — both ends cap these and drop rather than block -------------- */

#define OCP_MAX_LINE_LEN        512  /* excluding terminator */
#define OCP_MAX_ARGV            10   /* verb + 9 args        */
#define OCP_MAX_VERB_LEN        32
#define OCP_MAX_SSID_LEN        32
#define OCP_MAX_BSSID_STR_LEN   17
#define OCP_EVENT_QUEUE_DEPTH   32
#define OCP_MAX_FRAME_ROWS      256  /* larger results must be paged      */

/* --- Framing (OCP-SPEC §3) ------------------------------------------------ */

#define OCP_KW_BEGIN            "BEGIN"
#define OCP_KW_END              "END"

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
#define OCP_MARK_FT             "[FT]"   /* reserved, unimplemented (D-2) */

/* The only unbracketed reply; parsers must match it explicitly. */
#define OCP_REPLY_PONG          "pong"

/* --- Capabilities (OCP-SPEC §4) ------------------------------------------- */

/* All name receive capabilities. There is no transmit cap. */
#define OCP_CAP_WIFI24          "wifi24"
#define OCP_CAP_WIFI5           "wifi5"
#define OCP_CAP_BLE             "ble"
#define OCP_CAP_IEEE802154      "ieee802154"
#define OCP_CAP_LORA_RX         "lora_rx"

#define OCP_CAP_SEP             ","

/* Verbs gate on a class, not a single cap: OCP_CC_WIFI is satisfied by
 * either wifi24 or wifi5. */
typedef enum {
    OCP_CC_NONE = 0,
    OCP_CC_WIFI,
    OCP_CC_BLE,
    OCP_CC_IEEE802154,
    OCP_CC_LORA_RX
} ocp_cap_class_t;

/* --- Verbs (DESIGN Appendix A) -------------------------------------------- */

/* System */
#define OCP_V_HELLO             "hello"
#define OCP_V_PING              "ping"
#define OCP_V_VERSION           "version"
#define OCP_V_STATUS            "status"
#define OCP_V_STOP              "stop"
#define OCP_V_REBOOT            "reboot"

/* Wi-Fi */
#define OCP_V_SCAN_NETWORKS     "scan_networks"
#define OCP_V_START_WIFI_SCAN   "start_wifi_scan"
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
#define OCP_V_START_BLE_SCAN    "start_ble_scan"
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
 * The command table. Referencing the macros above keeps each verb string
 * defined once. The probe's dispatch table, the tooling and the §8 audit all
 * expand this list.
 *
 *   X(id, verb, cap_class, min_args, max_args, reply_marker)
 */
#define OCP_VERB_TABLE(X)                                                                          \
    /*   id              verb                     cap class          min max  reply            */  \
    X(HELLO,             OCP_V_HELLO,             OCP_CC_NONE,        0,  0,  OCP_MARK_HELLO)      \
    X(PING,              OCP_V_PING,              OCP_CC_NONE,        0,  0,  OCP_REPLY_PONG)      \
    X(VERSION,           OCP_V_VERSION,           OCP_CC_NONE,        0,  0,  OCP_MARK_VER)        \
    X(STATUS,            OCP_V_STATUS,            OCP_CC_NONE,        0,  0,  OCP_MARK_STATUS)     \
    X(STOP,              OCP_V_STOP,              OCP_CC_NONE,        0,  1,  OCP_MARK_STOP)       \
    X(REBOOT,            OCP_V_REBOOT,            OCP_CC_NONE,        0,  0,  "")                  \
    X(SCAN_NETWORKS,     OCP_V_SCAN_NETWORKS,     OCP_CC_WIFI,        0,  0,  OCP_MARK_SCAN)       \
    X(START_WIFI_SCAN,   OCP_V_START_WIFI_SCAN,   OCP_CC_WIFI,         0,  0,  OCP_MARK_CFG)        \
    X(SHOW_SCAN_RESULTS, OCP_V_SHOW_SCAN_RESULTS, OCP_CC_WIFI,        0,  1,  OCP_MARK_SCAN)       \
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
    X(START_BLE_SCAN,    OCP_V_START_BLE_SCAN,    OCP_CC_BLE,         0,  0,  OCP_MARK_CFG)        \
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

/* --- Errors (OCP-SPEC §5.3) ----------------------------------------------- */

#define OCP_ERR_BUSY            "busy"      /* PHY lane held; owner= names it  */
#define OCP_ERR_BADARG          "badarg"    /* wrong arity or range            */
#define OCP_ERR_BUDGET          "budget"    /* refused by the power interlock  */
#define OCP_ERR_HWFAULT         "hwfault"   /* peripheral did not respond      */
#define OCP_ERR_UNKNOWN         "unknown"   /* verb not in this command table  */
#define OCP_ERR_NOCAP           "nocap"     /* verb known, capability absent   */
#define OCP_ERR_INTERNAL        "internal"  /* probe-side bug                  */

/* --- Event kinds — additive; unknown kinds are ignored --------------------- */

#define OCP_EVT_KIND_SNIFF      "sniff"
#define OCP_EVT_KIND_FOLLOWER   "follower"
#define OCP_EVT_KIND_AIRTAG     "airtag"
#define OCP_EVT_KIND_BLE        "ble"       /* start_ble_scan: a newly seen device */
#define OCP_EVT_KIND_NETWORK   "network"   /* start_wifi_scan: a newly seen AP */
#define OCP_EVT_KIND_CHAN       "chan"
#define OCP_EVT_KIND_LORA       "lora"
#define OCP_EVT_KIND_CLIENT     "client"
#define OCP_EVT_KIND_PROBE      "probe"
#define OCP_EVT_KIND_DEAUTH     "deauth"
#define OCP_EVT_KIND_WARDRIVE   "wardrive"
#define OCP_EVT_KIND_ZIG        "zig"       /* first sighting from start_zig_recon */

/* --- Well-known keys and row shapes --------------------------------------- */

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
#define OCP_K_LANE              "lane"     /* [STOP]: which lane(s) it addressed */
#define OCP_K_UPTIME_MS         "uptime_ms"
#define OCP_K_TS_MS             "ts_ms"    /* monotonic probe timestamp */
#define OCP_K_HEAP              "heap"     /* [STATUS]: free heap, bytes  */
#define OCP_K_PKTS              "pkts"
#define OCP_K_MAC               "mac"
#define OCP_K_SSID              "ssid"
#define OCP_K_REASON            "reason"    /* deauth/disassoc reason code, as sent */
#define OCP_K_DISASSOC          "disassoc"  /* 1 = disassociation, 0 = deauthentication */

/* Wi-Fi survey frames (OCP-SPEC §10). */
#define OCP_K_TOTAL             "total"    /* results stored on the probe  */
#define OCP_K_FIRST             "first"    /* 1-based idx of a page's row 1 */
#define OCP_K_ABORTED           "aborted"  /* 1 if `stop` cut it short      */
#define OCP_K_DWELL_MS          "dwell_ms"
#define OCP_K_ELAPSED_MS        "elapsed_ms"
#define OCP_K_IDX               "idx"
#define OCP_K_BSSID             "bssid"
#define OCP_K_CH                "ch"
#define OCP_K_BAND              "band"
#define OCP_K_BEACONS           "beacons"
#define OCP_K_RSSI              "rssi"
#define OCP_K_RSN               "rsn"
#define OCP_K_PRIVACY           "privacy"  /* 802.11 capability privacy bit */
#define OCP_K_MFP_CAPABLE       "mfp_capable"
#define OCP_K_MFP_REQUIRED      "mfp_required"
#define OCP_K_UPTIME_S          "uptime_s"
#define OCP_K_INTERVAL_MS       "interval_ms"
#define OCP_K_FREQ              "freq"
#define OCP_K_SF                "sf"
#define OCP_K_BW                "bw"
#define OCP_K_CR                "cr"
#define OCP_K_SNR               "snr"
#define OCP_K_LEN               "len"
#define OCP_K_HEX               "hex"

/* 802.15.4 frames (OCP-SPEC §12). */
#define OCP_K_STATE             "state"
#define OCP_K_DROPPED           "dropped"
#define OCP_K_PANS              "pans"
#define OCP_K_NODES             "nodes"
#define OCP_ZIG_PAN_CSV_HEADER  "\"kind\",\"pan\",\"proto\",\"confidence\",\"channels\",\"nodes\",\"rssi\",\"lqi\""
#define OCP_ZIG_PAN_CSV_FIELDS  8
#define OCP_ZIG_NODE_CSV_HEADER "\"kind\",\"pan\",\"short\",\"ext\",\"role\",\"rssi\",\"lqi\",\"seen\""
#define OCP_ZIG_NODE_CSV_FIELDS 8
#define OCP_ZIG_ROW_PAN         "pan"
#define OCP_ZIG_ROW_NODE        "node"
#define OCP_ZIG_PROTO_UNKNOWN   "unknown"
#define OCP_ZIG_PROTO_802154    "802154"

/* BLE frames (OCP-SPEC §11). */
#define OCP_K_NAME              "name"      /* AD type 0x08/0x09, "" if absent */
#define OCP_K_MFR               "mfr"       /* AD type 0xFF company ID, 4 lowercase hex digits, "" if absent */
/* [BLE]'s "tracker" column: "" or one of the OCP_EVT_KIND_* tracker values
 * above (OCP_EVT_KIND_AIRTAG today) — the same classification `scan_airtag`
 * streams as events, just attached to a device-list row instead. Extensible:
 * only Apple's Find My network (AirTag and other FindMy-compatible
 * accessories) is classified in v1; other vendors' beacon formats are a
 * future addition, not a gap in this one. */
#define OCP_K_TRACKER           "tracker"

/* [SCAN] "auth" column values. */
#define OCP_AUTH_OPEN           "OPEN"
#define OCP_AUTH_WEP            "WEP"
#define OCP_AUTH_WPA            "WPA"
#define OCP_AUTH_WPA2           "WPA2"
#define OCP_AUTH_WPA_WPA2       "WPA/WPA2"
#define OCP_AUTH_WPA3           "WPA3"
#define OCP_AUTH_WPA2_WPA3      "WPA2/WPA3"
#define OCP_AUTH_WPA_EAP        "WPA-EAP"
#define OCP_AUTH_WPA2_EAP       "WPA2-EAP"
#define OCP_AUTH_WPA3_EAP       "WPA3-EAP"
#define OCP_AUTH_WPA2_WPA3_EAP  "WPA2/WPA3-EAP"
#define OCP_AUTH_WPA3_EAP192    "WPA3-EAP192"
#define OCP_AUTH_OWE            "OWE"
#define OCP_AUTH_WAPI           "WAPI"
#define OCP_AUTH_DPP            "DPP"
#define OCP_AUTH_UNKNOWN        "UNKNOWN"

/* [SCAN] "band" column values. */
#define OCP_BAND_LABEL_24       "2.4"
#define OCP_BAND_LABEL_5        "5"

/* Escaping rules: OCP-SPEC §6. */
#define OCP_SCAN_CSV_HEADER     "\"idx\",\"ssid\",\"bssid\",\"ch\",\"auth\",\"rssi\",\"band\""
#define OCP_SCAN_CSV_FIELDS     7

/* [CLIENTS] / [PROBES] rows (OCP-SPEC §10.4). */
#define OCP_CLIENTS_CSV_HEADER  "\"bssid\",\"mac\",\"ch\",\"band\",\"rssi\",\"pkts\""
#define OCP_CLIENTS_CSV_FIELDS  6
#define OCP_PROBES_CSV_HEADER   "\"mac\",\"ssid\",\"rssi\",\"pkts\""
#define OCP_PROBES_CSV_FIELDS   4

/* [BLE] rows (OCP-SPEC §11.2). */
#define OCP_BLE_CSV_HEADER      "\"mac\",\"name\",\"mfr\",\"tracker\",\"rssi\",\"n\""
#define OCP_BLE_CSV_FIELDS      6

#define OCP_BAND_24             "24"
#define OCP_BAND_5              "5"
#define OCP_BAND_AUTO           "auto"

/* PHY-lane owners, as reported by [STATUS] and [ERR] owner=. */
#define OCP_OWNER_NONE          "none"
#define OCP_OWNER_WIFI          "wifi"
#define OCP_OWNER_BLE           "ble"
#define OCP_OWNER_IEEE802154    "ieee802154"

/* `stop` scope: DESIGN §6.2's two arbiter lanes, plus "all" (D-16).
 * A bare `stop` means OCP_LANE_ALL, so older decks keep working. */
#define OCP_LANE_ALL            "all"
#define OCP_LANE_PHY            "phy"
#define OCP_LANE_LORA           "lora"

/* --- Helpers -------------------------------------------------------------- */

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
}
#endif

#endif /* OSCILLA_OCP_H */
