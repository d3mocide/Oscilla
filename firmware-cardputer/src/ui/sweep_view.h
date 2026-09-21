/*
 * sweep_view.h — the Wi-Fi Scan AP list (DESIGN §7.2).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "model/scan_model.h"
#include "ui/chrome.h"

namespace ui {

void drawSweepView(const model::ScanModel &scan, size_t cursor, uint32_t scanning_ms,
                   const ChromeState &chrome);

}  // namespace ui
