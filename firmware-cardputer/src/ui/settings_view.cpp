/*
 * settings_view.cpp — deck display luminance and Wi-Fi region; Observatory v2.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/settings_view.h"

#include <cstdio>
#include <cstring>

#include "ui/canvas.h"
#include "ui/observatory_chrome.h"
#include "ui/segmented_bar.h"
#include "ui/theme.h"

namespace ui {

namespace {

constexpr int kBarSegmentCount = 10;
constexpr int kBarSegmentWidth = 12;
constexpr int kBarSegmentGap = 2;
constexpr int kPanelX = 4;
constexpr int kPanelW = 232;
constexpr int kCharW = 6;   /* default 6x8 font at size 1 */

void panel(M5Canvas &d, int y, int h)
{
    d.fillRect(kPanelX, y, kPanelW, h, kPanelDark);
    d.drawRect(kPanelX, y, kPanelW, h, kLineBorder);
    d.fillRect(kPanelX, y, 28, 2, kFieldGreen);
}

}  // namespace

void drawSettingsView(uint8_t brightness, const char *region_code, const char *band_label,
                      bool band_ok, const char *region_channels, const char *region_status,
                      bool region_ok, const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    /* Brightness: label, hero percentage, segmented bar on the right. */
    const int top = kBodyTop + 2;
    panel(d, top, 44);
    d.setCursor(10, top + 6);
    d.setTextColor(kMutedSlate, kPanelDark);
    d.print("DISPLAY BRIGHTNESS");

    const unsigned pct = (brightness * 100U + 127U) / 255U;
    char pbuf[12];
    std::snprintf(pbuf, sizeof pbuf, "%u%%", pct);
    instrument::Canvas c(d, 0);
    observatory::obsText(c, 10, top + 18, pbuf, observatory::FontId::Hero, kCalibrationYellow, 80);

    const int filled = static_cast<int>((brightness * kBarSegmentCount + 254) / 255);
    drawSegmentedBar(d, 92, top + 24, kBarSegmentCount, kBarSegmentWidth, 9, kBarSegmentGap,
                     filled, kFieldGreen);

    /* Wi-Fi region + band: label and probe state, then code, band, and channels. */
    const int rtop = top + 48;
    panel(d, rtop, 32);
    d.setCursor(10, rtop + 6);
    d.setTextColor(kMutedSlate, kPanelDark);
    d.print("WI-FI REGION / BAND");
    const int status_w = static_cast<int>(std::strlen(region_status)) * kCharW;
    d.setCursor(kPanelX + kPanelW - 6 - status_w, rtop + 6);
    d.setTextColor(region_ok ? kFieldGreen : kCalibrationYellow, kPanelDark);
    d.print(region_status);

    d.setCursor(10, rtop + 19);
    d.setTextColor(kCalibrationYellow, kPanelDark);
    d.print(region_code);
    d.setTextColor(kMutedSlate, kPanelDark);
    d.print(" ");
    d.setTextColor(band_ok ? kFieldGreen : kCalibrationYellow, kPanelDark);
    d.print(band_label);
    d.setTextColor(kMutedSlate, kPanelDark);
    d.print("  ");
    d.print(region_channels);

    d.setCursor(6, rtop + 37);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print("; . BRT  , / REG  b BAND  ` LINK");

    endChrome(chrome);
}

}  // namespace ui
