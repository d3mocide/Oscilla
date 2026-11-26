/*
 * settings_view.cpp — scrollable deck configuration list; Observatory v2.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/settings_view.h"

#include <cstdio>
#include <string>

#include "ui/canvas.h"
#include "ui/display_text.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {

namespace {

constexpr int kCharW = 6;

void formatRowValue(const SettingsViewState &s, SettingsRow row, std::string &value, uint16_t &color)
{
    char buf[24];
    switch (row) {
    case SettingsRow::Brightness: {
        const unsigned pct = (s.brightness * 100U + 127U) / 255U;
        std::snprintf(buf, sizeof buf, "%u%%", pct);
        value = buf;
        color = kCalibrationYellow;
        return;
    }
    case SettingsRow::IdleSleep:
        value = s.idle_label;
        color = s.idle_timeout_s ? kFieldGreen : kMutedSlate;
        return;
    case SettingsRow::WifiRegion:
        value = s.region_code + (s.region_ok ? " OK" : " PEND");
        color = s.region_ok ? kFieldGreen : kCalibrationYellow;
        return;
    case SettingsRow::WifiBand:
        value = s.band_label;
        color = s.band_ok ? kFieldGreen : kCalibrationYellow;
        return;
    case SettingsRow::LoraProfile:
        value = printable(s.lora_label, 12);
        color = s.lora_from_sd ? kCalibrationYellow : kFieldGreen;
        return;
    case SettingsRow::AudioCues:
        value = s.audio_enabled ? "ON" : "MUTED";
        color = s.audio_enabled ? kCalibrationYellow : kMutedSlate;
        return;
    case SettingsRow::DebugSerial:
        value = s.debug_enabled ? "ON" : "OFF";
        color = s.debug_enabled ? kSignalPink : kMutedSlate;
        return;
    case SettingsRow::Count:
        break;
    }
    value = "--";
    color = kMutedSlate;
}

}  // namespace

void drawSettingsView(const SettingsViewState &s, const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(5, kBodyTop + 3);
    d.setTextColor(kFieldGreen, kVoidInk);
    d.print("CONFIGURATION");

    char idx_buf[16];
    const size_t cur = s.cursor < kSettingsRowCount ? s.cursor : 0;
    std::snprintf(idx_buf, sizeof idx_buf, "%u/%u",
                  static_cast<unsigned>(cur + 1), static_cast<unsigned>(kSettingsRowCount));
    const int idx_w = static_cast<int>(std::char_traits<char>::length(idx_buf)) * kCharW;
    d.setCursor(235 - idx_w, kBodyTop + 3);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print(idx_buf);
    d.drawFastHLine(5, kBodyTop + 14, 230, kDimGreen);

    const size_t start = listFirstVisible(cur, kListVisibleRows);
    for (size_t i = 0; i < static_cast<size_t>(kListVisibleRows) && start + i < kSettingsRowCount; ++i) {
        const size_t row_idx = start + i;
        const auto row = static_cast<SettingsRow>(row_idx);
        const bool sel = row_idx == cur;
        const int y = kBodyTop + 19 + static_cast<int>(i) * kListRowHeight;
        const uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);

        d.setCursor(6, y);
        d.setTextColor(sel ? kCalibrationYellow : kPaperPhosphor, bg);
        d.print(settingsRowName(row));

        std::string val;
        uint16_t vcol = kPaperPhosphor;
        formatRowValue(s, row, val, vcol);
        const int vw = static_cast<int>(val.size()) * kCharW;
        d.setCursor(234 - vw, y);
        d.setTextColor(sel ? kCalibrationYellow : vcol, bg);
        d.print(val.c_str());
    }

    d.drawFastHLine(5, kBodyTop + 86, 230, kDimGreen);
    d.setCursor(5, kHintRowY);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print(cur == static_cast<size_t>(SettingsRow::AudioCues)
                ? "; . ROW  ENTER TOGGLE  ` LINK"
                : "; . ROW  , / ADJUST  ` LINK");

    endChrome(chrome);
}

}  // namespace ui
