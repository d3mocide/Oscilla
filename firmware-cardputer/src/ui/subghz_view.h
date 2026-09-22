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

/* pending_profile_label names whichever model::kLoraProfiles[] entry 'c'
 * would apply next (cycled with 'x') — shown so the operator knows what
 * they're about to select before committing it. */
void drawSubGhzView(const model::LoraModel &lora, size_t cursor, const ChromeState &chrome,
                    const char *pending_profile_label);

}  // namespace ui
