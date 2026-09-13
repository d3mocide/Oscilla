/*
 * trace_view.h — one AP in depth (DESIGN §7.2 "Trace").
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

#include "model/scan_model.h"

namespace ui {

/* `row` may be null if the probe rebooted and the list was cleared. */
void drawTraceView(const model::ApRow *row, const model::Inspect &inspect, bool listening,
                   const std::string &notice);

}  // namespace ui
