/*
 * chrome.cpp — shared Cardputer 240x135 Observatory v2 header & transport footer.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/chrome.h"

#include <algorithm>
#include <cstdio>

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/observatory_chrome.h"
#include "ui/theme.h"

namespace ui {

namespace {

std::string clipped(const std::string &value, size_t max_len)
{
    if (value.size() <= max_len) return value;
    if (max_len < 2) return value.substr(0, max_len);
    return value.substr(0, max_len - 1) + ">";
}

void drawIndicator(M5Canvas &d, int x, IndicatorState state)
{
    d.drawCircle(x, 9, 4, kVoidInk);
    d.fillCircle(x, 9, 3, indicatorColour(state));
}

void drawBattery(M5Canvas &d, int pct, bool charging)
{
    d.setTextSize(1);
    d.setTextColor(pct < 0 ? kMutedSlate : pct <= 20 ? kFaultRed : kPaperPhosphor, kPanelDark);
    char percent[4];
    if (pct < 0) std::snprintf(percent, sizeof percent, "%3s", "--");
    else std::snprintf(percent, sizeof percent, "%3d", std::max(0, std::min(100, pct)));
    d.setCursor(194, 6);
    d.print(percent);

    const uint16_t color = pct >= 0 && pct <= 20 ? kFaultRed : kPaperPhosphor;
    d.fillRect(218, 4, 16, 10, kVoidInk);
    d.drawRect(218, 4, 16, 10, color);
    d.fillRect(234, 7, 2, 4, color);
    if (pct > 0) {
        int width = std::max(1, std::min(12, pct * 12 / 100));
        d.fillRect(220, 6, width, 6, pct <= 20 ? kFaultRed : kFieldGreen);
    }
    if (charging) d.fillRect(214, 6, 2, 6, kCalibrationYellow);
}

}  // namespace

uint16_t indicatorColour(IndicatorState state)
{
    switch (state) {
    case IndicatorState::Ready:
    case IndicatorState::Active:
        return kFieldGreen;
    case IndicatorState::Pending:
        return kCalibrationYellow;
    case IndicatorState::Fault:
        return kFaultRed;
    case IndicatorState::Off:
        return kDimGreen;
    }
    return kMutedSlate;
}

void beginChrome(const ChromeState &state)
{
    auto &d = ui::canvas();
    d.fillScreen(kVoidInk);
    d.fillRect(0, 0, d.width(), kHeaderHeight, kPanelDark);
    d.setTextSize(1);

    instrument::Canvas c(d, 0);
    observatory::obsText(c, 4, 5, observatory::obsUpper(state.title),
                         observatory::FontId::Head, kPaperPhosphor,
                         state.debug ? 102 : 134);

    if (state.debug) {
        d.fillRect(110, 4, 24, 11, kSelectionGlow);
        d.drawRect(110, 4, 24, 11, kCalibrationYellow);
        d.setTextColor(kCalibrationYellow, kSelectionGlow);
        d.setCursor(113, 6);
        d.print("DBG");
    }

    drawIndicator(d, 146, state.link == ocp::LinkState::Ready ? IndicatorState::Ready :
                               state.link == ocp::LinkState::HelloSent ? IndicatorState::Pending : IndicatorState::Fault);
    drawIndicator(d, 159, state.gnss);
    drawIndicator(d, 172, state.storage);
    drawIndicator(d, 185, state.rf);
    drawBattery(d, state.battery_pct, state.charging);

    d.drawFastHLine(0, kBodyTop - 1, d.width(), kDimGreen);
    d.fillRect(0, kBodyTop - 2, 26, 2, kFieldGreen);
    d.fillRect(0, kBodyTop, d.width(), kBodyHeight, kVoidInk);
}

void endChrome(const ChromeState &state)
{
    auto &d = ui::canvas();
    d.setTextSize(1);

    if (!state.notice.empty()) {
        d.fillRect(0, kFooterTop, d.width(), kFooterHeight, kSelectionGlow);
        d.drawFastHLine(0, kFooterTop, d.width(), kCalibrationYellow);
        d.fillRect(0, kFooterTop, 2, kFooterHeight, kCalibrationYellow);
        d.setCursor(5, kFooterTop + 3);
        d.setTextColor(kCalibrationYellow, kSelectionGlow);
        d.print(clipped(state.notice, 38).c_str());
        return;
    }

    d.fillRect(0, kFooterTop, d.width(), kFooterHeight, kPanelDark);
    d.drawFastHLine(0, kFooterTop, d.width(), kLineBorder);

    const char *lbl = state.cached ? "CACHED" : state.live ? "LIVE" : "IDLE";
    const uint16_t fill = state.cached ? kVoidInk : state.live ? kTrackDark : kVoidInk;
    const uint16_t border = state.cached ? kSignalPink : state.live ? kFieldGreen : kLineBorder;
    const uint16_t fg = state.cached ? kSignalPink : state.live ? kFieldGreen : kMutedSlate;
    const int pw = state.cached ? 42 : 30;
    d.fillRect(2, kFooterTop + 2, pw, 11, fill);
    d.drawRect(2, kFooterTop + 2, pw, 11, border);
    d.setCursor(5, kFooterTop + 4);
    d.setTextColor(fg, fill);
    d.print(lbl);

    d.setCursor(pw + 6, kFooterTop + 4);
    d.setTextColor(kMutedSlate, kPanelDark);
    const size_t max_chars = static_cast<size_t>((d.width() - (pw + 8)) / 6);
    d.print(clipped(state.transport, max_chars).c_str());
}

}  // namespace ui
