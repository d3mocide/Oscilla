/*
 * bt_view.h — the Beacons screen (DESIGN §7.2): live BLE device list from
 * scan_bt, plus a running Find My / AirTag tracker count and sighting log
 * from scan_airtag.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <string>

#include "model/bt_model.h"
#include "model/anti_surveillance_model.h"

namespace ui {

void drawBtView(const model::BtModel &bt, const model::AntiSurveillanceModel &anti,
                size_t cursor, const std::string &notice);

}  // namespace ui
