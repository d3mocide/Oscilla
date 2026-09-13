/*
 * deauth_view.h — the Deauth card: a live log of deauth/disassoc detections
 * (deauth_detector, OCP-SPEC §10.5).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <string>

#include "model/deauth_model.h"

namespace ui {

void drawDeauthView(const model::DeauthModel &deauth, size_t cursor, const std::string &notice);

}  // namespace ui
