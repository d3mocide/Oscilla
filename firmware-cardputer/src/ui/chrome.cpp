/*
 * chrome.cpp — see chrome.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/chrome.h"

#include <algorithm>
#include <cstdio>

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/theme.h"

namespace ui {

namespace {

uint16_t indicatorColor(IndicatorState state)
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

std::string clipped(const std::string &value, size_t max_len)
{
    if (value.size() <= max_len) return value;
    if (max_len < 2) return value.substr(0, max_len);
    return value.substr(0, max_len - 1) + ">";
}

void drawIndicator(M5Canvas &d, int x, IndicatorState state)
{
    d.fillCircle(x, 9, 3, indicatorColor(state));
}

void drawBattery(M5Canvas &d, int pct, bool charging)
{
    d.setTextColor(pct < 0 ? kMutedSlate : pct <= 20 ? kFaultRed : kPaperPhosphor, kVoidInk);
    char percent[4];
    if (pct < 0) std::snprintf(percent, sizeof percent, "%3s", "--");
    else std::snprintf(percent, sizeof percent, "%3d", std::max(0, std::min(100, pct)));
    d.setCursor(194, 5);
    d.print(percent);

    const uint16_t color = pct >= 0 && pct <= 20 ? kFaultRed : kPaperPhosphor;
    d.drawRect(218, 4, 16, 10, color);
    d.fillRect(234, 7, 2, 4, color);
    if (pct > 0) {
        int width = std::max(1, std::min(12, pct * 12 / 100));
        d.fillRect(220, 6, width, 6, pct <= 20 ? kFaultRed : kFieldGreen);
    }
    if (charging) d.fillRect(214, 6, 2, 6, kCalibrationYellow);
}

}  // namespace

void beginChrome(const ChromeState &state)
{
    auto &d = ui::canvas();
    d.fillScreen(kVoidInk);
    d.setTextSize(1);

    d.setTextColor(kPaperPhosphor, kVoidInk);
    d.setCursor(2, 5);
    d.print(clipped(state.title, 16).c_str());

    drawIndicator(d, 146, state.link == ocp::LinkState::Ready ? IndicatorState::Ready :
                               state.link == ocp::LinkState::HelloSent ? IndicatorState::Pending : IndicatorState::Fault);
    drawIndicator(d, 159, state.gnss);
    drawIndicator(d, 172, state.storage);
    drawIndicator(d, 185, state.rf);
    if (state.debug) {
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.setCursor(112, 5);
        d.print("DBG");
    }
    drawBattery(d, state.battery_pct, state.charging);

    d.drawFastHLine(0, kBodyTop - 1, d.width(), kDimGreen);
    d.fillRect(0, kBodyTop, d.width(), kBodyHeight, kVoidInk);
}

void endChrome(const ChromeState &state)
{
    auto &d = ui::canvas();
    d.fillRect(0, kFooterTop, d.width(), kFooterHeight, kVoidInk);
    d.drawFastHLine(0, kFooterTop, d.width(), kLineBorder);
    d.setCursor(3, kFooterTop + 3);

    if (!state.notice.empty()) {
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print(clipped(state.notice, 38).c_str());
        return;
    }

    d.setTextColor(state.cached ? kSignalPink : state.live ? kFieldGreen : kMutedSlate, kVoidInk);
    d.print(state.cached ? "CACHED" : state.live ? "LIVE" : "IDLE");
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print("  ");
    d.print(clipped(state.transport, 30).c_str());
}

}  // namespace ui
