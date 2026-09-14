/*
 * wardrive_csv.h — WigleWifi-1.6 CSV formatting for the wardrive log
 * (DESIGN §9.2: "WigleWifi-1.6 CSV — geolocated Wi-Fi/BLE survey rows
 * (WiGLE-compatible)."). Pure formatting, no SD/file I/O — same split as
 * storage/lora_log_format.h. Host-tested in test/host/wardrive_csv_test.cpp.
 *
 * The header/column layout is verified against Kismet's `kis_wiglecsvlogfile.cc`
 * (kismetwireless/kismet, fetched 2026-09-14) — the only concrete source
 * found for the exact "WigleWifi-1.6" tag (WiGLE's own Android app parses an
 * older, shorter 11-column variant with no version line in its own client
 * source; "1.6" specifically names Kismet's 14-column extension with
 * RCOIs/MfgrId). One deliberate deviation from that source: Kismet's own
 * Wi-Fi row only emits 12 of its declared 14 columns (RCOIs/MfgrId are
 * silently missing there, unlike its own BT/BLE rows, which do emit them
 * blank) — an apparent bug in their shipped code. Oscilla's rows always
 * emit all 14 columns, RCOIs/MfgrId blank, for internal consistency with
 * the declared header.
 *
 * Oscilla's ApRow (model/scan_model.h) is coarser than Kismet's raw 802.11
 * element data: no cipher-suite (CCMP/TKIP) or AKM detail beyond OCP's
 * OCP_AUTH_* buckets. authModeTag() below maps those buckets to
 * WigleWifi-style bracket tags at the *method* level only (e.g.
 * "[WPA2-PSK][ESS]", not "[WPA2-PSK-CCMP][ESS]") — a narrower tag than the
 * richest real-world exports, not a wrong one.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

#include "model/scan_model.h"

namespace storage {

/* Session metadata line, written once at file creation. `fw_version` is
 * Oscilla's own deck firmware version (OSCILLA_DECK_VER). */
std::string wardriveCsvHeader(const std::string &fw_version);

/* Column header, written once right after wardriveCsvHeader(). */
std::string wardriveCsvColumnHeader();

/* One AP observation row. Date/time fields are the GNSS fix's own UTC
 * calendar time (year full, e.g. 2026; month/day 1-based; hour/min/sec
 * 0-based) — the caller's job to supply, same explicit-clock convention as
 * storage::loraLogRow's ts_ms and ocp::Client's now_ms. accuracy_m is
 * whatever the caller can derive from the fix (HDOP-to-meters conversion
 * isn't done here — no verified UERE constant for the GNSS unit in use);
 * passing 0 when unknown matches real-world exports that do the same. */
std::string wardriveCsvRow(const model::ApRow &ap,
                           int year, int month, int day, int hour, int min, int sec,
                           double lat, double lon, double alt_m, float accuracy_m);

}  // namespace storage
