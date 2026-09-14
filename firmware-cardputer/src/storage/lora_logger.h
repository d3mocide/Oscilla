/*
 * lora_logger.h — writes the LoRa session CSV to the deck's microSD
 * (DESIGN §9.2). One file per `lora_listen` session; row shape lives in
 * lora_log_format.h.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "model/lora_model.h"

namespace storage {

/* Opens a new session file under /oscilla/lora/ (numbered, since there's no
 * wall-clock source to name it by). False if SD isn't ready or no unused
 * filename was found — logging is then simply unavailable this session,
 * never blocking or retrying against the UI. */
bool loraLogBegin();

/* Appends one row and flushes immediately (crash-tolerant, same as DESIGN
 * §9.2's KML — LoRa traffic is bursty but not so dense that per-row flush
 * is a real cost yet). No-op if no session is open. */
void loraLogPacket(uint32_t freq_hz, int sf, int bw_khz, int cr, const model::LoraPacket &p);

void loraLogEnd();

}  // namespace storage
