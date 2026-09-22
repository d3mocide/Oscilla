/*
 * legacy_view.h — the CC1101 RX screen (mirrors ui/subghz_view.h): a live
 * legacy_listen capture log, RSSI/length per row, no framing guess (raw
 * capture only, see model/legacy_model.h).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

#include "model/legacy_model.h"
#include "ui/chrome.h"

namespace ui {

void drawLegacyView(const model::LegacyModel &legacy, size_t cursor, const ChromeState &chrome);

}  // namespace ui
