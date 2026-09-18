/*
 * sweep_view.cpp — see sweep_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/sweep_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/dwell_animation.h"
#include "ui/link_view.h"
#include "ui/theme.h"

namespace ui {

namespace {

constexpr int kSweepChannelX = 126;
constexpr int kSweepBandX = 154;
constexpr int kSweepRssiX = 186;
constexpr int kSweepMeterX = 216;

uint16_t rssiColour(int rssi)
{
    if (rssi >= -55) return kFieldGreen;
    if (rssi >= -70) return kCalibrationYellow;
    return kFaultRed;
}

void drawMeter(M5Canvas &d, int x, int y, int rssi)
{
    int filled = (rssi + 100) * 5 / 70;
    if (filled < 0) filled = 0;
    if (filled > 5) filled = 5;
    for (int i = 0; i < 5; ++i) {
        d.fillRect(x + i * 4, y, 3, 5, i < filled ? rssiColour(rssi) : kTrackDark);
    }
}

}  // namespace

void drawSweepView(const model::ScanModel &scan, size_t cursor, uint32_t scanning_ms,
                   const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    if (scan.continuousActive()) {
        d.printf("TOTAL %u APs  LIVE %us", (unsigned)scan.rows().size(), (unsigned)(scanning_ms / 1000));
    } else if (scan.scanning()) {
        d.printf("SCAN IN PROGRESS  %us", (unsigned)(scanning_ms / 1000));
    } else if (scan.aborted()) {
        d.print("SCAN STOPPED");
    } else {
        d.printf("TOTAL %u APs  %us", (unsigned)scan.rows().size(), (unsigned)(scan.elapsedMs() / 1000));
    }
    d.setTextColor(kMutedSlate, kVoidInk);
    d.setCursor(178, kBodyTop + 2);
    d.printf("%s", scan.rows().empty() ? "--" : "RX");

    const auto &rows = scan.rows();
    const int list_top = kBodyTop + 16;
    const int row_height = 13;
    const int visible = 4;
    size_t first = cursor >= (size_t)visible ? cursor - visible + 1 : 0;

    for (int i = 0; i < visible && first + i < rows.size(); i++) {
        const auto &r = rows[first + i];
        bool sel = first + i == cursor;
        int y = list_top + i * row_height;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        if (sel) {
            d.fillRect(0, y - 2, d.width(), row_height, bg);
            d.fillRect(0, y - 2, 2, row_height, kCalibrationYellow);
        }

        std::string name = r.ssid.empty() ? "<hidden>" : printable(r.ssid, 15);
        d.setCursor(6, y);
        d.setTextColor(r.ssid.empty() ? kDimGreen : kPaperPhosphor, bg);
        d.printf("%-15s", name.c_str());
        d.setTextColor(kMutedSlate, bg);
        d.setCursor(kSweepChannelX, y);
        d.printf("%3u", r.ch);
        d.setCursor(kSweepBandX, y);
        d.printf("%4s", r.band5 ? "5G" : "2.4G");
        d.setTextColor(rssiColour(r.rssi), bg);
        d.setCursor(kSweepRssiX, y);
        d.printf("%4d", r.rssi);
        drawMeter(d, kSweepMeterX, y + 1, r.rssi);
    }

    if (rows.empty()) {
        d.setCursor(4, list_top);
        d.setTextColor(kDimGreen, kVoidInk);
        if (scan.scanning()) drawDwellAnimation(d, scanning_ms);
        else d.print("NO OBSERVATIONS YET");
    }

    d.setCursor(4, kBodyTop + 83);
    const bool live = scan.scanning() || scan.continuousActive();
    if (cursor < rows.size()) {
        const auto &r = rows[cursor];
        d.setTextColor(live ? kFieldGreen : kMutedSlate, kVoidInk);
        d.printf("CH %u %s · %d dBm · %s", r.ch, r.band5 ? "5G" : "2.4G", r.rssi,
                 printable(r.auth, 8).c_str());
    } else if (scan.scanning()) {
        /* The centered dwell animation is the only bounded-sweep progress
         * indicator; do not duplicate it in the lower body row. */
    } else if (scan.aborted()) {
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print("SCAN ABORTED · NO SNAPSHOT");
    } else {
        d.setTextColor(kMutedSlate, kVoidInk);
        d.print(rows.empty() ? "NO AP OBSERVATIONS" : "SELECT AN AP ROW");
    }

    endChrome(chrome);
}

}  // namespace ui
