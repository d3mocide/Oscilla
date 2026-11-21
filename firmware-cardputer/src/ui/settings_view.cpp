/*
 * settings_view.cpp — deck display luminance control; Observatory v2.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/settings_view.h"

#include <cstdio>

#include "ui/canvas.h"
#include "ui/observatory_chrome.h"
#include "ui/segmented_bar.h"
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

    const unsigned pct = (brightness * 100U + 127U) / 255U;
    d.fillRect(4, kBodyTop + 4, 232, 54, kPanelDark);
    d.drawRect(4, kBodyTop + 4, 232, 54, kLineBorder);
    d.fillRect(4, kBodyTop + 4, 28, 2, kFieldGreen);

    d.setCursor(10, kBodyTop + 10);
    d.setTextColor(kMutedSlate, kPanelDark);
    d.print("DISPLAY BRIGHTNESS");

    char pbuf[12];
    std::snprintf(pbuf, sizeof pbuf, "%u%%", pct);
    instrument::Canvas c(d, 0);
    observatory::obsText(c, 10, kBodyTop + 22, pbuf, observatory::FontId::Hero, kCalibrationYellow, 96);

    const int bar_x = 10;
    const int bar_y = kBodyTop + 43;
    const int filled = static_cast<int>((brightness * kBarSegmentCount + 254) / 255);
    drawSegmentedBar(d, bar_x, bar_y, kBarSegmentCount, kBarSegmentWidth, 9, kBarSegmentGap,
                     filled, kFieldGreen);

    d.setCursor(6, kBodyTop + 66);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print("; .   BRIGHTER / DIMMER");
    d.setCursor(6, kBodyTop + 79);
    d.print("`     BACK TO LINK");

    endChrome(chrome);
}

}  // namespace ui
