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
    size_t first = listFirstVisible(cursor, kListCompactRows);

    for (int i = 0; i < kListCompactRows && first + static_cast<size_t>(i) < events.size(); i++) {
        const auto &e = events[first + i];
        bool sel = first + static_cast<size_t>(i) == cursor;
        int y = kListTop + i * kListRowHeight;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);

        d.setCursor(6, y);
        d.setTextColor(e.disassoc ? kCalibrationYellow : kSignalPink, bg);
        d.printf("%-17s", printable(e.mac, 17).c_str());
        d.setCursor(114, y);
        d.setTextColor(kMutedSlate, bg);
        if (e.ch) d.printf("C%-2u", e.ch);
        else d.print("C--");
        d.setCursor(138, y);
        d.printf("R%-3ld", e.reason);
        d.setCursor(168, y);
        d.setTextColor(e.disassoc ? kCalibrationYellow : kSignalPink, bg);
        d.print(e.disassoc ? "DIS" : "DEA");
        d.setCursor(204, y);
        d.setTextColor(rssiColour(e.rssi), bg);
        d.printf("%4d", e.rssi);
    }

    if (events.empty()) {
        d.setTextColor(kDimGreen, kVoidInk);
        d.setCursor(4, kListTop);
        d.print(deauth.active() ? "LISTENING FOR MANAGEMENT FRAMES..." : "NO DETECTIONS YET");
    }

    d.setCursor(4, kHintRowY);
    d.setTextColor(kCalibrationYellow, kVoidInk);
    d.print("s WATCH-STOP / ; . ROW");

    endChrome(chrome);
}

}  // namespace ui
