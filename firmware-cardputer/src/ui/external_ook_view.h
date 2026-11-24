/*
 * external_ook_view.h — the 433 MHz OOK RX card on the external panel
 * (OCP-SPEC §15, D-20): one lane per transmitter with a ten-minute
 * heard-when timeline, and the selected transmitter's reading in the dock.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <M5GFX.h>

#include "model/ook_model.h"

namespace ui {

void drawExternalOokStrip(lgfx::LGFX_Sprite &d, int top, const model::OokModel &m, size_t cursor, uint32_t now);

}  // namespace ui
