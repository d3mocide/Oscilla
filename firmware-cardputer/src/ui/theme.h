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

/* RGB565 semantic colors. The deck's reference background is true TFT black. */
constexpr uint16_t kVoidInk = 0x0000;             /* physical reference black */
constexpr uint16_t kPaperPhosphor = 0xF75B;      /* #F4ECD9 */
constexpr uint16_t kFieldGreen = 0xAEEC;          /* #A8DF65 */
constexpr uint16_t kSignalPink = 0xF3D3;          /* #F05D9D */
constexpr uint16_t kCalibrationYellow = 0xF54B;  /* #F5D658 */
constexpr uint16_t kFaultRed = 0xEA89;            /* #EF514C */
constexpr uint16_t kMutedSlate = 0xAD14;          /* #AAA0A5 */
constexpr uint16_t kLineBorder = 0x49E9;          /* #4B3C49 */
constexpr uint16_t kDimGreen = 0x53A7;            /* #51763D */
constexpr uint16_t kTrackDark = 0x21A5;           /* #263528 */
constexpr uint16_t kPanelDark = 0x1083;           /* #151018 */
constexpr uint16_t kSelectionGlow = 0x20E1;       /* #241F0D */

constexpr int kHeaderTop = 0;
constexpr int kHeaderHeight = 20;
constexpr int kBodyTop = kHeaderTop + kHeaderHeight;
constexpr int kBodyHeight = 101;
constexpr int kFooterHeight = 14;
constexpr int kFooterTop = kBodyTop + kBodyHeight;

}  // namespace ui
