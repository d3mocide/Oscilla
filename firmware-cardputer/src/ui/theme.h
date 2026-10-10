/*
 * theme.h — solid 16-bit deck colors and fixed physical layout constants.
 *
 * Values follow docs/brand/README.md. The names are intentionally semantic:
 * color is allowed to reinforce meaning, never replace a text state.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace ui {

/* CSS colors from the rendered master guide, quantized once to RGB565. */
constexpr uint16_t rgb565(uint32_t hex) {
    return ((hex >> 8) & 0xF800) | ((hex >> 5) & 0x07E0) | ((hex >> 3) & 0x001F);
}
constexpr uint16_t kVoidInk = rgb565(0x0B090D);
constexpr uint16_t kPaperPhosphor = rgb565(0xF4ECD9);
constexpr uint16_t kFieldGreen = rgb565(0xA8DF65);
constexpr uint16_t kSignalPink = rgb565(0xF05D9D);
constexpr uint16_t kCalibrationYellow = rgb565(0xF5D658);
constexpr uint16_t kFaultRed = rgb565(0xEF514C);
constexpr uint16_t kMutedSlate = rgb565(0x9DAF96);
constexpr uint16_t kLineBorder = rgb565(0x4B3C49);
constexpr uint16_t kDimGreen = rgb565(0x51763D);
constexpr uint16_t kTrackDark = rgb565(0x263528);
constexpr uint16_t kPanelDark = rgb565(0x151018);
constexpr uint16_t kSelectionGlow = rgb565(0x241F0D);

constexpr int kHeaderTop = 0;
constexpr int kHeaderHeight = 20;
constexpr int kBodyTop = kHeaderTop + kHeaderHeight;
constexpr int kBodyHeight = 101;
constexpr int kFooterHeight = 14;
constexpr int kFooterTop = kBodyTop + kBodyHeight;

}  // namespace ui
