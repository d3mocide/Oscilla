/*
 * legacy_view.cpp — see legacy_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/legacy_view.h"

#include <cstdio>

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {

void drawLegacyView(const model::LegacyModel &legacy, size_t cursor, const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.printf("PACKETS %u  %s", (unsigned)legacy.total(), legacy.active() ? "LISTENING" : "IDLE");
    if (legacy.hasConfig()) {
        char freq[16];
        std::snprintf(freq, sizeof freq, "%lu.%03lu",
                      (unsigned long)(legacy.freqHz() / 1000000UL),
                      (unsigned long)((legacy.freqHz() % 1000000UL) / 1000UL));
        d.setCursor(164, kBodyTop + 2);
        d.print(freq);
    } else {
        d.setCursor(164, kBodyTop + 2);
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print("UNCONFIGURED");
    }

    const auto &packets = legacy.packets();
    size_t first = listFirstVisible(cursor);

    for (int i = 0; i < kListVisibleRows && first + static_cast<size_t>(i) < packets.size(); i++) {
        const auto &p = packets[first + i];
        bool sel = first + static_cast<size_t>(i) == cursor;
        int y = kListTop + i * kListRowHeight;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);

        d.setCursor(6, y);
        d.setTextColor(rssiColour(p.rssi), bg);
        d.printf("%4d", p.rssi);
        d.setTextColor(kMutedSlate, bg);
        d.printf(" %3u ", p.len);
        d.setTextColor(kDimGreen, bg);
        d.print(printable(p.hex, 26).c_str());
    }

    if (packets.empty()) {
        d.setTextColor(kDimGreen, kVoidInk);
        d.setCursor(0, kListTop);
        d.print(legacy.active() ? "LISTENING FOR CC1101 RX..."
                                 : legacy.hasConfig() ? "PRESS s TO START" : "PRESS c TO CONFIGURE");
    }

    d.setCursor(4, kDetailRowY);
    if (legacy.hasConfig()) {
        char freq[16];
        std::snprintf(freq, sizeof freq, "%lu.%03lu",
                      (unsigned long)(legacy.freqHz() / 1000000UL),
                      (unsigned long)((legacy.freqHz() % 1000000UL) / 1000UL));
        d.setTextColor(legacy.active() ? kFieldGreen : kMutedSlate, kVoidInk);
        d.printf("%s N%u", freq, (unsigned)legacy.total());
    } else {
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print("CFG REQUIRED · PRESS c");
    }

    endChrome(chrome);
}

}  // namespace ui
