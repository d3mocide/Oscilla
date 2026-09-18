/*
 * settings_view.cpp — see settings_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/settings_view.h"

#include "ui/canvas.h"
#include "ui/theme.h"

namespace ui {

namespace {

constexpr int kBarSegmentCount = 10;
constexpr int kBarSegmentWidth = 18;
constexpr int kBarSegmentGap = 3;

}  // namespace

void drawSettingsView(uint8_t brightness, const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(4, kBodyTop + 3);
    d.setTextColor(kFieldGreen, kVoidInk);
    d.print("DISPLAY BRIGHTNESS");

    d.setTextSize(2);
    d.setCursor(4, kBodyTop + 18);
    d.setTextColor(kPaperPhosphor, kVoidInk);
    d.printf("%3u%%", (unsigned)((brightness * 100U + 127U) / 255U));
    d.setTextSize(1);

    const int bar_x = 4;
    const int bar_y = kBodyTop + 42;
    const int filled = static_cast<int>((brightness * kBarSegmentCount + 254) / 255);
    for (int i = 0; i < kBarSegmentCount; ++i) {
        d.fillRect(bar_x + i * (kBarSegmentWidth + kBarSegmentGap), bar_y,
                   kBarSegmentWidth, 10, i < filled ? kFieldGreen : kTrackDark);
    }

    d.setCursor(4, kBodyTop + 62);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print("; .   BRIGHTER / DIMMER");
    d.setCursor(4, kBodyTop + 75);
    d.print("`     BACK TO LINK");

    endChrome(chrome);
}

}  // namespace ui
