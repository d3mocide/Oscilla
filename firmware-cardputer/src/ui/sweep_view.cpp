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
#include "ui/list_row.h"
#include "ui/segmented_bar.h"
#include "ui/theme.h"

namespace ui {

namespace {

constexpr int kSweepChannelX = 126;
constexpr int kSweepBandX = 154;
constexpr int kSweepRssiX = 186;
constexpr int kSweepMeterX = 216;

void drawMeter(M5Canvas &d, int x, int y, int rssi)
{
    int filled = (rssi + 100) * 5 / 70;
    if (filled < 0) filled = 0;
    if (filled > 5) filled = 5;
    drawSegmentedBar(d, x, y, 5, 3, 5, 1, filled, rssiColour(rssi));
}

}  // namespace

void drawSweepView(const model::ScanModel &scan, size_t cursor, uint32_t scanning_ms,
                   const ChromeState &chrome, const model::BaselineWatch *baseline)
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
    const bool bl_on = baseline && baseline->active();
    d.setCursor(178, kBodyTop + 2);
    if (bl_on) {
        const size_t hi = baseline->highSeverityCount();
        const size_t tot = baseline->anomalies().size();
        d.setTextColor(hi ? kSignalPink : tot ? kCalibrationYellow : kFieldGreen, kVoidInk);
        if (hi) d.printf("BL !%u", (unsigned)hi);
        else if (tot) d.printf("BL +%u", (unsigned)tot);
        else d.print("BL OK");
    } else {
        d.setTextColor(kMutedSlate, kVoidInk);
        d.printf("%s", scan.rows().empty() ? "--" : "RX");
    }

    const auto &rows = scan.rows();
    size_t first = listFirstVisible(cursor, kListCompactRows);

    for (int i = 0; i < kListCompactRows && first + i < rows.size(); i++) {
        const auto &r = rows[first + i];
        bool sel = first + i == cursor;
        int y = kListTop + i * kListRowHeight;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);

        const auto *anom = bl_on ? baseline->anomalyForBssid(r.bssid) : nullptr;
        std::string name = r.ssid.empty() ? "<hidden>" : printable(r.ssid, 15);
        uint16_t name_col = r.ssid.empty() ? kDimGreen : kPaperPhosphor;
        if (anom) {
            name_col = model::BaselineWatch::isHighSeverity(anom->kind) ? kSignalPink : kCalibrationYellow;
        }
        d.setCursor(6, y);
        d.setTextColor(name_col, bg);
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
        if (scan.scanning()) {
            drawDwellAnimation(d, scanning_ms);
        } else if (scan.aborted()) {
            d.setCursor(4, kListTop);
            d.setTextColor(kCalibrationYellow, kVoidInk);
            d.print("SCAN ABORTED / NO SNAPSHOT");
        } else {
            d.setCursor(4, kListTop);
            d.setTextColor(kDimGreen, kVoidInk);
            d.print("NO OBSERVATIONS YET");
        }
    }

    d.setCursor(4, kHintRowY);
    d.setTextColor(kCalibrationYellow, kVoidInk);
    d.print(bl_on ? "ENTER INFO / r SCAN / c LIVE / b CLR" : "ENTER INFO / r SCAN / c LIVE / b BASE");
    endChrome(chrome);
}

}  // namespace ui
