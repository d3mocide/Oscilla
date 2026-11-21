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

void drawDeviceRows(const model::BtModel &bt, size_t cursor, int visible_rows)
{
    auto &d = ui::canvas();
    const auto &rows = bt.devices();
    size_t first = listFirstVisible(cursor, static_cast<size_t>(visible_rows));

    for (int i = 0; i < visible_rows && first + i < rows.size(); i++) {
        const auto &r = rows[first + i];
        bool sel = first + i == cursor;
        int y = kListTop + i * kListRowHeight;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);

        std::string label = r.name.empty() ? printable(r.mac, 22) : printable(r.name, 22);
        d.setCursor(6, y);
        d.setTextColor(r.name.empty() ? kDimGreen : kPaperPhosphor, bg);
        d.printf("%-22s", label.c_str());
        d.setCursor(156, y);
        d.setTextColor(r.tracker ? kSignalPink : kMutedSlate, bg);
        d.print(r.tracker ? model::trackerShortLabel(r.tracker_kind) : "   ");
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

    const bool show_tracker = !bt.trackerHits().empty();
    drawDeviceRows(bt, cursor, show_tracker ? kListVisibleRows : kListCompactRows);
    const bool bounded_dwell = bt.scanning() && bt.devices().empty();
    if (bounded_dwell) {
        drawDwellAnimation(d, scanning_ms);
    } else if (bt.devices().empty()) {
        d.setCursor(4, kListTop);
        d.setTextColor(listening ? kFieldGreen : kDimGreen, kVoidInk);
        d.print(listening ? "LISTENING FOR BLE ADVERTISEMENTS..." : "NO BLE DEVICES");
    }

    if (show_tracker) {
        d.setCursor(4, kDetailRowY);
        const auto &last = bt.trackerHits()[0];
        d.setTextColor(kSignalPink, kVoidInk);
        d.printf("LAST %s %s %d", model::trackerShortLabel(last.tracker_kind),
                 printable(last.mac, 17).c_str(), last.rssi);
    }

    d.setCursor(4, kHintRowY);
    d.setTextColor(kCalibrationYellow, kVoidInk);
    d.print("ENTER DOSSIER / s SCAN / c LIVE");
    endChrome(chrome);
}

}  // namespace ui
