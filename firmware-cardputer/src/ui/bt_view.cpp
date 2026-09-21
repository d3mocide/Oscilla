/*
 * bt_view.cpp — see bt_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/bt_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/dwell_animation.h"
#include "ui/link_view.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {

namespace {

void drawDeviceRows(const model::BtModel &bt, size_t cursor)
{
    auto &d = ui::canvas();
    const auto &rows = bt.devices();
    size_t first = listFirstVisible(cursor);

    for (int i = 0; i < kListVisibleRows && first + i < rows.size(); i++) {
        const auto &r = rows[first + i];
        bool sel = first + i == cursor;
        int y = kListTop + i * kListRowHeight;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);

        std::string label = r.name.empty() ? printable(r.mac, 18) : printable(r.name, 18);
        d.setCursor(6, y);
        d.setTextColor(r.name.empty() ? kDimGreen : kPaperPhosphor, bg);
        d.printf("%-18s", label.c_str());
        d.setCursor(156, y);
        d.setTextColor(r.tracker ? kSignalPink : kMutedSlate, bg);
        d.print(r.tracker ? "TRK" : "   ");
        d.setCursor(196, y);
        d.setTextColor(rssiColour(r.rssi), bg);
        d.printf("%4d", r.rssi);
    }
}

}  // namespace

void drawBtView(const model::BtModel &bt, size_t cursor, uint32_t scanning_ms,
                const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    const bool listening = bt.scanning() || bt.continuousActive() || bt.airtagActive();
    const unsigned tracker_count = bt.airtagActive() ? (unsigned)bt.trackerCount()
                                                      : (unsigned)bt.deviceTrackerCount();
    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.printf("DEVICES %u", (unsigned)bt.devices().size());
    d.setCursor(132, kBodyTop + 2);
    d.printf("TRACKERS ");
    d.setTextColor(tracker_count ? kSignalPink : kMutedSlate, kVoidInk);
    d.printf("%u", tracker_count);
    if (bt.malformedRows()) d.printf("  BAD %u", bt.malformedRows());

    drawDeviceRows(bt, cursor);
    const bool bounded_dwell = bt.scanning() && bt.devices().empty();
    if (bounded_dwell) {
        drawDwellAnimation(d, scanning_ms);
    } else if (!bt.trackerHits().empty()) {
        d.setCursor(4, kDetailRowY);
        const auto &last = bt.trackerHits()[0];
        d.setTextColor(kMutedSlate, kVoidInk);
        d.printf("LAST TRACKER %s %d", printable(last.mac, 17).c_str(), last.rssi);
    } else if (cursor < bt.devices().size()) {
        d.setCursor(4, kDetailRowY);
        const auto &row = bt.devices()[cursor];
        const std::string label = row.name.empty() ? row.mac : row.name;
        d.setTextColor(kMutedSlate, kVoidInk);
        d.print(printable(label, 18).c_str());
        d.setTextColor(rssiColour(row.rssi), kVoidInk);
        d.printf("  %d dBm", row.rssi);
        d.setTextColor(kMutedSlate, kVoidInk);
        d.printf(" · %u seen", (unsigned)row.n);
    } else if (listening) {
        d.setCursor(4, kDetailRowY);
        d.setTextColor(kFieldGreen, kVoidInk);
        d.printf("DEVICES %u · TRACKERS %u", (unsigned)bt.devices().size(),
                 tracker_count);
    } else {
        d.setCursor(4, kDetailRowY);
        d.setTextColor(kMutedSlate, kVoidInk);
        d.print("NO BLE DEVICES");
    }

    endChrome(chrome);
}

}  // namespace ui
