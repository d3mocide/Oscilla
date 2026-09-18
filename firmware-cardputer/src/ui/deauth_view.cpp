/*
 * deauth_view.cpp — see deauth_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/deauth_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/theme.h"

namespace ui {

namespace {

uint16_t rssiColour(int rssi)
{
    if (rssi >= -55) return kFieldGreen;
    if (rssi >= -70) return kCalibrationYellow;
    return kFaultRed;
}

}  // namespace

void drawDeauthView(const model::DeauthModel &deauth, size_t cursor, const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.printf("DETECTIONS %u  %s", (unsigned)deauth.totalCount(), deauth.active() ? "WATCHING" : "IDLE");

    const auto &events = deauth.events();
    constexpr int list_top = kBodyTop + 16;
    constexpr int row_height = 13;
    constexpr int visible = 4;
    size_t first = cursor >= static_cast<size_t>(visible) ? cursor - visible + 1 : 0;

    for (int i = 0; i < visible && first + static_cast<size_t>(i) < events.size(); i++) {
        const auto &e = events[first + i];
        bool sel = first + static_cast<size_t>(i) == cursor;
        int y = list_top + i * row_height;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        if (sel) {
            d.fillRect(0, y - 2, d.width(), row_height, bg);
            d.fillRect(0, y - 2, 2, row_height, kCalibrationYellow);
        }

        /* Colour carries deauth-vs-disassoc so the text stays this compact. */
        d.setCursor(0, y);
        d.setTextColor(e.disassoc ? kCalibrationYellow : kSignalPink, bg);
        d.printf("%c%-15s", sel ? '>' : ' ', printable(e.mac, 15).c_str());
        d.setTextColor(kMutedSlate, bg);
        d.printf(" r%-3ld", e.reason);
        d.setTextColor(rssiColour(e.rssi), bg);
        d.printf(" %4d", e.rssi);
    }

    if (events.empty()) {
        d.setTextColor(kDimGreen, kVoidInk);
        d.setCursor(0, list_top);
        d.print(deauth.active() ? "LISTENING FOR MANAGEMENT FRAMES..." : "NO DETECTIONS YET");
    }

    d.setCursor(4, kBodyTop + 83);
    if (!events.empty() && cursor < events.size()) {
        const auto &e = events[cursor];
        d.setTextColor(kMutedSlate, kVoidInk);
        d.printf("%s R%ld %d dBm", printable(e.mac, 17).c_str(), e.reason, e.rssi);
    } else {
        d.setTextColor(deauth.active() ? kFieldGreen : kMutedSlate, kVoidInk);
        d.printf("EVENTS %u · %s", (unsigned)events.size(), deauth.active() ? "WATCHING" : "IDLE");
    }

    endChrome(chrome);
}

}  // namespace ui
