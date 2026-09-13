/*
 * sweep_view.h — the AP list (DESIGN §7.2 "Sweep").
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <string>

#include "model/scan_model.h"

namespace ui {

void drawSweepView(const model::ScanModel &scan, size_t cursor, uint32_t scanning_ms,
                   const std::string &notice);

}  // namespace ui
