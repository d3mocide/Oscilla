/*
 * gnss_view.h — the Drive card (DESIGN §7.2): GNSS fix state and the
 * wardrive logging session.
 *
 * This is P4's exit-gate instrument, not decoration: the gate is
 * demonstrated by *looking* at this screen while the antenna is unplugged,
 * so the four model::GnssState values must stay visually distinct here.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "model/gnss_model.h"
#include "ui/chrome.h"

namespace ui {

void drawGnssView(const model::GnssModel &gnss, uint32_t now_ms, const ChromeState &chrome);

}  // namespace ui
