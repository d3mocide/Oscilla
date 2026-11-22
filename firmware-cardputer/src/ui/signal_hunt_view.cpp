/*
 * signal_hunt_view.cpp — see signal_hunt_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/signal_hunt_view.h"

#include <algorithm>
#include <cstdio>

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/display_text.h"
#include "ui/theme.h"

namespace ui {

void drawSignalHuntView(const model::SignalHunt &h, uint32_t now_ms, const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    const bool stale = h.isStale(now_ms);
    const std::string title = h.label().empty() ? h.mac() : printable(h.label(), 20);
    d.setCursor(5, kBodyTop + 3);
    d.setTextColor(kPaperPhosphor, kVoidInk);
    d.print(title.c_str());

    char tag[20];
    if (h.kind() == model::HuntKind::WifiAp) std::snprintf(tag, sizeof tag, "WIFI CH%u", unsigned(h.channel()));
    else std::snprintf(tag, sizeof tag, "BT LE");
    const int tw = static_cast<int>(std::char_traits<char>::length(tag)) * 6;
    d.setCursor(235 - tw, kBodyTop + 3);
    d.setTextColor(kFieldGreen, kVoidInk);
    d.print(tag);
    d.drawFastHLine(5, kBodyTop + 14, 230, kDimGreen);

    /* Large RSSI readout + proximity state. */
    d.setCursor(6, kBodyTop + 20);
    if (!stale) {
        const uint16_t rcol = h.currentRssi() >= -50 ? kSignalPink
                            : h.currentRssi() >= -68 ? kCalibrationYellow : kFieldGreen;
        d.setTextColor(rcol, kVoidInk);
        d.printf("%d dBm  %s", h.currentRssi(), model::SignalHunt::proximityLabel(h.currentRssi(), false));
    } else {
        d.setTextColor(kMutedSlate, kVoidInk);
        d.print(h.samples() ? "SIGNAL LOST" : "WAITING FOR TARGET");
    }

    char stat[28];
    if (h.samples()) std::snprintf(stat, sizeof stat, "PK %d  #%lu", h.peakRssi(), (unsigned long)h.samples());
    else std::snprintf(stat, sizeof stat, "AUDIO %s", h.audioEnabled() ? "ON" : "MUTE");
    const int sw = static_cast<int>(std::char_traits<char>::length(stat)) * 6;
    d.setCursor(235 - sw, kBodyTop + 20);
    d.setTextColor(kCalibrationYellow, kVoidInk);
    d.print(stat);

    /* Calibrated proximity bar (-100..-35 dBm). */
    constexpr int bx = 6, by = kBodyTop + 34, bw = 228, bh = 10;
    d.fillRect(bx, by, bw, bh, kTrackDark);
    if (!stale) {
        const int clamped = std::max(-100, std::min(-35, h.currentRssi()));
        const int fill = std::max(4, (clamped + 100) * bw / 65);
        const uint16_t bcol = h.currentRssi() >= -50 ? kSignalPink
                            : h.currentRssi() >= -68 ? kCalibrationYellow : kFieldGreen;
        d.fillRect(bx, by, fill, bh, bcol);
    }

    /* Compact RSSI sparkline (y = body+48 .. body+82). */
    constexpr int sx = 6, sy = kBodyTop + 48, sw_px = 228, sh = 34;
    d.drawFastHLine(sx, sy + sh, sw_px, kDimGreen);
    const size_t n = h.historyCount();
    if (n > 1) {
        for (size_t i = 1; i < n; ++i) {
            const int x0 = sx + static_cast<int>((i - 1) * sw_px / (model::SignalHunt::kHistoryCap - 1));
            const int x1 = sx + static_cast<int>(i * sw_px / (model::SignalHunt::kHistoryCap - 1));
            const int v0 = std::max(-105, std::min(-35, static_cast<int>(h.historyAt(i - 1))));
            const int v1 = std::max(-105, std::min(-35, static_cast<int>(h.historyAt(i))));
            const int y0 = sy + sh - 1 - (v0 + 105) * (sh - 2) / 70;
            const int y1 = sy + sh - 1 - (v1 + 105) * (sh - 2) / 70;
            d.drawLine(x0, y0, x1, y1, stale ? kMutedSlate : kCalibrationYellow);
        }
    }

    d.drawFastHLine(5, kBodyTop + 86, 230, kDimGreen);
    d.setCursor(5, kBodyTop + 89);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.printf("a %s  r PEAK  ` EXIT HUNT", h.audioEnabled() ? "MUTE" : "AUDIO");
    endChrome(chrome);
}

}  // namespace ui
