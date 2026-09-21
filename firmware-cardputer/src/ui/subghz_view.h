/*
 * subghz_view.h — the LoRa RX screen (DESIGN §7.2): a live LoRa packet log
 * (lora_listen), RSSI/SNR per row.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

#include "model/lora_model.h"
#include "ui/chrome.h"

namespace ui {

void drawSubGhzView(const model::LoraModel &lora, size_t cursor, const ChromeState &chrome);

}  // namespace ui
