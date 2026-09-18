/*
 * anti_surveillance_view.h — bounded passive tracker-correlation renderer.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

#include "model/anti_surveillance_model.h"
#include "ui/chrome.h"

namespace ui {

void drawAntiSurveillanceView(const model::AntiSurveillanceModel &anti, size_t cursor,
                              bool starting, const ChromeState &chrome);

}  // namespace ui
