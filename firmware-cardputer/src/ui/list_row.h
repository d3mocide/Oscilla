/*
 * list_row.h — shared windowed-list layout, selection highlight, and RSSI
 * color helper for card views that render a scrollable 4-row list.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include <M5Cardputer.h>

#include "ui/theme.h"

namespace ui {

constexpr int kListTop = kBodyTop + 16;
constexpr int kListRowHeight = 13;
constexpr int kListVisibleRows = 4;
constexpr int kDetailRowY = kBodyTop + 83;

/* First visible row index for a scroll window that follows the cursor. */
inline size_t listFirstVisible(size_t cursor, size_t visible = kListVisibleRows)
{
    return cursor >= visible ? cursor - visible + 1 : 0;
}

/* Selected-row glow + left accent bar. No-op when not selected. */
inline void drawRowHighlight(M5Canvas &d, int y, bool selected, int row_height = kListRowHeight)
{
    if (!selected) return;
    d.fillRect(0, y - 2, d.width(), row_height, kSelectionGlow);
    d.fillRect(0, y - 2, 2, row_height, kCalibrationYellow);
}

/* Common RSSI-to-signal-quality color used across scan/observation views. */
inline uint16_t rssiColour(int rssi)
{
    if (rssi >= -55) return kFieldGreen;
    if (rssi >= -70) return kCalibrationYellow;
    return kFaultRed;
}

}  // namespace ui
