/*
 * lora_session_format.h — pure sidecar formats for a LoRa RX session.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

#include "model/lora_model.h"

namespace storage {

std::string loraSessionManifest(uint32_t started_ms, const model::LoraModel &lora);
std::string loraHealthHeader();
std::string loraHealthRow(uint32_t ts_ms, const model::LoraHealth &health,
                          uint32_t packet_rows, uint32_t packet_drops,
                          uint32_t health_drops);

}  // namespace storage
