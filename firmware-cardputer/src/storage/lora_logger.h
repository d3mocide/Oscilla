/*
 * lora_logger.h — writes the LoRa session CSV to the deck's microSD
 * (DESIGN §9.2). One file per `lora_listen` session; row shape lives in
 * lora_log_format.h.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "model/gnss_model.h"
#include "model/lora_model.h"

namespace storage {

struct LoraLogStats {
    bool open = false;
    uint32_t packet_rows = 0;
    uint32_t packet_drops = 0;
    uint32_t health_rows = 0;
    uint32_t health_drops = 0;
    uint32_t geotagged_rows = 0;
    uint32_t waypoints = 0;
};

/* Opens numbered packet, manifest, health, and KML sidecars under /oscilla/lora/.
 * False if SD isn't ready or no unused filename was found — logging is then
 * simply unavailable this session, never blocking or retrying against the UI. */
bool loraLogBegin(const model::LoraModel &lora);

/* Appends one row and flushes immediately; also emits a KML placemark when
 * fix is non-null and valid. No-op if no session is open. */
void loraLogPacket(uint32_t freq_hz, int sf, int bw_khz, int cr, const model::LoraPacket &p,
                   const model::GnssFix *fix = nullptr, uint32_t fix_age_ms = 0);

/* Drops a manual waypoint placemark into the open LoRa session KML. */
bool loraLogWaypoint(const model::GnssFix &fix, uint32_t *out_index = nullptr);

/* Appends a complete C5 health snapshot. No-op until a valid status reply
 * exists; packet and health write drops remain separately counted. */
void loraLogHealth(const model::LoraHealth &health);

void loraLogEnd();

const LoraLogStats &loraLogStats();

enum class LoraReadbackState { Unavailable, TooLarge, Mismatch, Match };
struct LoraLogReadback {
    LoraReadbackState state = LoraReadbackState::Unavailable;
    uint32_t packet_bytes = 0, health_bytes = 0;
};
/* Diagnostic only: compare open session files to accepted writes, <=8 KiB each. */
LoraLogReadback verifyLoraLog();

}  // namespace storage
