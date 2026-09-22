/*
 * legacy_log_format.h — pure CSV row formatting for the CC1101 session log
 * (mirrors storage/lora_log_format.h). No SD/file I/O here on purpose —
 * kept framework-agnostic so it's host-tested in
 * test/host/legacy_log_format_test.cpp.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

#include "model/legacy_model.h"

namespace storage {

/* Column header, written once per session file. */
std::string legacyLogHeader();

/* One row for one received chunk. ts_ms is the deck's own millis() at
 * receipt — no wall-clock source yet, same boot-relative-timestamp
 * philosophy as loraLogRow. */
std::string legacyLogRow(uint32_t ts_ms, uint32_t freq_hz, const model::LegacyChunk &c);

}  // namespace storage
