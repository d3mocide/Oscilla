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

struct LoraLogStats {
    bool open = false;
    uint32_t packet_rows = 0;
    uint32_t packet_drops = 0;
    uint32_t health_rows = 0;
    uint32_t health_drops = 0;
};

/* Opens numbered packet, manifest, and health sidecars under /oscilla/lora/.
 * False if SD isn't ready or no unused filename was found — logging is then
 * simply unavailable this session, never blocking or retrying against the UI. */
bool loraLogBegin(const model::LoraModel &lora);

/* Appends one row and flushes immediately (crash-tolerant, same as DESIGN
 * §9.2's KML — LoRa traffic is bursty but not so dense that per-row flush
 * is a real cost yet). No-op if no session is open. */
void loraLogPacket(uint32_t freq_hz, int sf, int bw_khz, int cr, const model::LoraPacket &p);

/* Appends a complete C5 health snapshot. No-op until a valid status reply
 * exists; packet and health write drops remain separately counted. */
void loraLogHealth(const model::LoraHealth &health);

void loraLogEnd();

const LoraLogStats &loraLogStats();

}  // namespace storage
