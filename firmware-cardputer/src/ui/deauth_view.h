/*
 * deauth_view.h — the Deauth Detect card: a live log of deauth/disassoc detections
 * (deauth_detector, OCP-SPEC §10.5).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>

#include "model/deauth_model.h"
#include "ui/chrome.h"

namespace ui {

void drawDeauthView(const model::DeauthModel &deauth, size_t cursor, const ChromeState &chrome);

}  // namespace ui
