/*
 * deauth_view.cpp — see deauth_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/deauth_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {

void drawDeauthView(const model::DeauthModel &deauth, size_t cursor, const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.printf("DETECTIONS %u  %s", (unsigned)deauth.totalCount(), deauth.active() ? "WATCHING" : "IDLE");

    const auto &events = deauth.events();
    size_t first = listFirstVisible(cursor);

    for (int i = 0; i < kListVisibleRows && first + static_cast<size_t>(i) < events.size(); i++) {
        const auto &e = events[first + i];
        bool sel = first + static_cast<size_t>(i) == cursor;
        int y = kListTop + i * kListRowHeight;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);

        /* Colour carries deauth-vs-disassoc so the text stays this compact. */
        d.setCursor(6, y);
        d.setTextColor(e.disassoc ? kCalibrationYellow : kSignalPink, bg);
        d.printf("%-15s", printable(e.mac, 15).c_str());
        d.setTextColor(kMutedSlate, bg);
        d.printf(" r%-3ld", e.reason);
        d.setTextColor(rssiColour(e.rssi), bg);
        d.printf(" %4d", e.rssi);
    }

    if (events.empty()) {
        d.setTextColor(kDimGreen, kVoidInk);
        d.setCursor(0, kListTop);
        d.print(deauth.active() ? "LISTENING FOR MANAGEMENT FRAMES..." : "NO DETECTIONS YET");
    }

    d.setCursor(4, kDetailRowY);
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
