/*
 * gnss_fix_view.h — the GNSS card: the receiver's fix on its own, without
 * the wardrive session (that is the Drive card, gnss_view.h). The external
 * panel shows the fix reticle (external_gnss_view.h) while this card is up.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include "model/gnss_model.h"
#include "ui/chrome.h"

namespace ui {

void drawGnssFixView(const model::GnssModel &gnss, uint32_t now_ms, const ChromeState &chrome);

}  // namespace ui
