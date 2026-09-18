/*
 * anti_surveillance_view.cpp — see anti_surveillance_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/anti_surveillance_view.h"

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

void drawAntiSurveillanceView(const model::AntiSurveillanceModel &anti, size_t cursor,
                              bool starting, const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.printf("TRACKERS %u", (unsigned)anti.trackers().size());
    d.setCursor(164, kBodyTop + 2);
    d.setTextColor(starting ? kCalibrationYellow : anti.active() ? kFieldGreen : kMutedSlate,
                   kVoidInk);
    d.print(starting ? "STARTING" : anti.active() ? "WATCH" : "IDLE");

    const auto &trackers = anti.trackers();
    constexpr int list_top = kBodyTop + 16;
    constexpr int row_height = 13;
    constexpr int visible = 4;
    const size_t first = cursor >= static_cast<size_t>(visible) ? cursor - visible + 1 : 0;

    for (int row = 0; row < visible && first + static_cast<size_t>(row) < trackers.size(); ++row) {
        const auto &tracker = trackers[first + static_cast<size_t>(row)];
        const bool sel = first + static_cast<size_t>(row) == cursor;
        const int y = list_top + row * row_height;
        const uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        if (sel) {
            d.fillRect(0, y - 2, d.width(), row_height, bg);
            d.fillRect(0, y - 2, 2, row_height, kCalibrationYellow);
        }

        d.setCursor(6, y);
        d.setTextColor(tracker.alert ? kSignalPink : kPaperPhosphor, bg);
        d.printf("%-15s", printable(tracker.mac, 15).c_str());
        d.setCursor(102, y);
        d.setTextColor(kMutedSlate, bg);
        d.printf("L%u", (unsigned)tracker.movement_legs);
        d.setCursor(132, y);
        d.printf("N%lu", (unsigned long)tracker.sightings);
        d.setCursor(204, y);
        d.setTextColor(rssiColour(tracker.rssi), bg);
        d.printf("%4d", tracker.rssi);
    }

    d.setCursor(4, kBodyTop + 83);
    if (const auto *alert = anti.latestAlert()) {
        d.setTextColor(kSignalPink, kVoidInk);
        d.printf("FOLLOW? %s · %u LEGS", printable(alert->mac, 17).c_str(),
                 (unsigned)alert->movement_legs);
    } else if (starting) {
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print("AWAITING PASSIVE TRACKER STREAM");
    } else if (anti.active()) {
        d.setTextColor(anti.hasPosition() ? kFieldGreen : kCalibrationYellow, kVoidInk);
        d.printf("%s · NEED 2 x 25m LEGS", anti.hasPosition() ? "FIX READY" : "NO FRESH FIX");
    } else if (!trackers.empty() && cursor < trackers.size()) {
        const auto &tracker = trackers[cursor];
        d.setTextColor(kMutedSlate, kVoidInk);
        d.printf("%s · %u/%u LEGS · %.0fm", printable(tracker.mac, 17).c_str(),
                 (unsigned)tracker.movement_legs,
                 (unsigned)model::AntiSurveillanceModel::kAlertMovementLegs,
                 tracker.traveled_m);
    } else if (!trackers.empty()) {
        d.setTextColor(kMutedSlate, kVoidInk);
        d.printf("%u TRACKERS · RESULTS HELD IN RAM", (unsigned)trackers.size());
    } else {
        d.setTextColor(kDimGreen, kVoidInk);
        d.print("PASSIVE ONLY · PRESS s TO WATCH");
    }

    endChrome(chrome);
}

}  // namespace ui
