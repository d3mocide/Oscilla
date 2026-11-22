/*
 * external_hunt_view.h — Observatory v2 320x240 Signal Hunt companion card
 * (WP-12b).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include <M5GFX.h>

#include "model/signal_hunt.h"

namespace ui {

void drawExternalHuntStrip(lgfx::LGFX_Sprite &strip, int top,
                           const model::SignalHunt &hunt, uint32_t now_ms);

}  // namespace ui
