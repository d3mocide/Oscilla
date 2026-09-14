/*
 * lora_log_format.h — pure CSV row formatting for the LoRa session log
 * (DESIGN §9.2: "CSV of packet observations with radio params"). No SD/file
 * I/O here on purpose — kept framework-agnostic so it's host-tested in
 * test/host/lora_log_format_test.cpp, same split as ocp/ocp_csv.h.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

#include "model/lora_model.h"

namespace storage {

/* Column header, written once per session file. */
std::string loraLogHeader();

/* One row for one received packet. ts_ms is the deck's own millis() at
 * receipt — there's no wall-clock time source (no GPS yet, no RTC), so this
 * is a boot-relative timestamp, same monotonic-timestamp philosophy DESIGN
 * §9.1 already uses for probe observations. */
std::string loraLogRow(uint32_t ts_ms, uint32_t freq_hz, int sf, int bw_khz, int cr,
                       const model::LoraPacket &p);

}  // namespace storage
