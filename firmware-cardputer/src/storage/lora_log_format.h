/*
 * lora_log_format.h — pure CSV and KML formatting for the LoRa session log
 * (DESIGN §9.2, WP-10). No SD/file I/O here on purpose — kept
 * framework-agnostic so it is host-tested in test/host/lora_log_format_test.cpp.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

#include "model/gnss_model.h"
#include "model/lora_model.h"

namespace storage {

/* Column header, written once per session CSV file (16 columns, additive
 * GNSS fields after the legacy 10 columns). */
std::string loraLogHeader();

/* One row for one received packet. When fix is non-null and valid, appends
 * local GNSS lat,lon,alt,hdop,fix_age_ms,utc; otherwise leaves the six GNSS
 * columns empty so every row keeps the same 16-column schema. */
std::string loraLogRow(uint32_t ts_ms, uint32_t freq_hz, int sf, int bw_khz, int cr,
                       const model::LoraPacket &p,
                       const model::GnssFix *fix = nullptr,
                       uint32_t fix_age_ms = 0);

/* Incremental KML 2.2 coverage log (all colors aabbggrr, zero remote <href>). */
std::string loraKmlHeader(const std::string &session_name);
std::string loraKmlPacketPlacemark(uint32_t ts_ms, uint32_t freq_hz, int sf, int bw_khz, int cr,
                                   const model::LoraPacket &p,
                                   double lat, double lon, double alt_m);
std::string loraKmlWaypointPlacemark(uint32_t index, uint32_t ts_ms,
                                     double lat, double lon, double alt_m,
                                     const std::string &note);
std::string loraKmlFooter();
std::string loraKmlRecoverySuffix(const std::string &tail);

}  // namespace storage
