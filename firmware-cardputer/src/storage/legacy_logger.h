/*
 * legacy_logger.h — writes the CC1101 session CSV to the deck's microSD,
 * mirroring storage/lora_logger.h. One file per `legacy_listen` session;
 * row shape lives in legacy_log_format.h.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "model/legacy_model.h"

namespace storage {

/* Opens a new session file under /oscilla/legacy/ (numbered, since there's
 * no wall-clock source to name it by). False if SD isn't ready or no unused
 * filename was found — logging is then simply unavailable this session,
 * never blocking or retrying against the UI. */
bool legacyLogBegin();

/* Appends one row and flushes immediately (crash-tolerant, same as
 * lora_logger.h — post-squelch CC1101 traffic is expected to be sparse, so
 * per-row flush cost is a non-issue). No-op if no session is open. */
void legacyLogPacket(uint32_t freq_hz, const model::LegacyPacket &p);

void legacyLogEnd();

}  // namespace storage
