/*
 * signal_hunt_view.h — onboard 240x135 single-target Signal Hunt view (WP-12b).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "model/signal_hunt.h"
#include "ui/chrome.h"

namespace ui {

void drawSignalHuntView(const model::SignalHunt &hunt, uint32_t now_ms, const ChromeState &chrome);

}  // namespace ui
