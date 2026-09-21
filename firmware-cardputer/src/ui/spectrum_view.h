/*
 * spectrum_view.h — the Packet Monitor screen (DESIGN §7.2): a live per-channel
 * activity bar chart (channel_view), or one channel's packets/sec reading
 * once locked (packet_monitor).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

#include "model/spectrum_model.h"
#include "ui/chrome.h"

namespace ui {

void drawSpectrumView(const model::SpectrumModel &spectrum, size_t cursor, const ChromeState &chrome);

}  // namespace ui
