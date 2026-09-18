/*
 * trace_view.h — one AP in depth (DESIGN §7.2 "AP Detail").
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

#include "model/scan_model.h"
#include "ui/chrome.h"

namespace ui {

/* `row` may be null if the probe rebooted and the list was cleared. */
void drawTraceView(const model::ApRow *row, const model::Inspect &inspect, bool listening,
                   const ChromeState &chrome);

}  // namespace ui
