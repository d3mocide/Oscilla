/*
 * subghz_view.h — the Sub-GHz screen (DESIGN §7.2): a live LoRa packet log
 * (lora_listen), RSSI/SNR per row.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <string>

#include "model/lora_model.h"

namespace ui {

void drawSubGhzView(const model::LoraModel &lora, size_t cursor, const std::string &notice);

}  // namespace ui
