/*
 * spectrum_view.h — the Spectrum screen (DESIGN §7.2): a live per-channel
 * activity bar chart (channel_view), or one channel's packets/sec reading
 * once locked (packet_monitor).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <string>

#include "model/spectrum_model.h"

namespace ui {

void drawSpectrumView(const model::SpectrumModel &spectrum, size_t cursor, const std::string &notice);

}  // namespace ui
