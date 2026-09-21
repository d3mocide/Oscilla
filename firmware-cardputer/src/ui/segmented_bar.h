/*
 * segmented_bar.h — shared discrete-segment meter/gauge drawing.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include <M5Cardputer.h>

#include "ui/theme.h"

namespace ui {

/* Draws `count` segments left to right starting at (x, y), `filled` of them
 * in fill_color and the rest in empty_color. */
inline void drawSegmentedBar(M5Canvas &d, int x, int y, int count, int seg_w, int seg_h,
                             int gap, int filled, uint16_t fill_color,
                             uint16_t empty_color = kTrackDark)
{
    for (int i = 0; i < count; ++i) {
        d.fillRect(x + i * (seg_w + gap), y, seg_w, seg_h, i < filled ? fill_color : empty_color);
    }
}

}  // namespace ui
