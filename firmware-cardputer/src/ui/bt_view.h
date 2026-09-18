/*
 * bt_view.h — the BLE Scan screen (DESIGN §7.2): live BLE device list from
 * scan_bt, plus a running Find My / AirTag tracker count and sighting log
 * from scan_airtag.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "model/bt_model.h"
#include "ui/chrome.h"

namespace ui {

void drawBtView(const model::BtModel &bt, size_t cursor, uint32_t scanning_ms,
                const ChromeState &chrome);

}  // namespace ui
