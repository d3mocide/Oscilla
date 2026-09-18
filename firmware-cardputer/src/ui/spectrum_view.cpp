/*
 * spectrum_view.cpp — see spectrum_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/spectrum_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/theme.h"

namespace ui {

namespace {

uint16_t barColour(uint32_t pkts, uint32_t max_pkts)
{
    if (max_pkts == 0) return kMutedSlate;
    float frac = static_cast<float>(pkts) / static_cast<float>(max_pkts);
    if (frac > 0.66f) return kSignalPink;
    if (frac > 0.33f) return kCalibrationYellow;
    return kFieldGreen;
}

void drawLocked(const model::SpectrumModel &spectrum)
{
    auto &d = ui::canvas();
    uint32_t pkts = spectrum.readings().empty() ? 0 : spectrum.readings()[0].pkts;

    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kFieldGreen, kVoidInk);
    d.printf("PACKET MONITOR LIVE  CH %u", spectrum.lockedChannel());
    d.setTextSize(2);
    d.setCursor(4, kBodyTop + 22);
    d.setTextColor(kCalibrationYellow, kVoidInk);
    d.printf("%u", (unsigned)pkts);
    d.setTextSize(1);
    d.setCursor(4, kBodyTop + 45);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print("PACKETS / SEC  ·  PRESS ` TO RELEASE");
}

void drawBroad(const model::SpectrumModel &spectrum, size_t cursor)
{
    auto &d = ui::canvas();
    const auto &readings = spectrum.readings();

    uint32_t max_pkts = 1;
    for (const auto &r : readings) if (r.pkts > max_pkts) max_pkts = r.pkts;

    constexpr int chart_top = kBodyTop + 4;
    constexpr int chart_bottom = kBodyTop + 72;
    int chart_h = chart_bottom - chart_top;
    int bar_w = readings.empty() ? 1 : d.width() / static_cast<int>(readings.size());
    if (bar_w > 14) bar_w = 14;
    if (bar_w < 1) bar_w = 1;
    int chart_w = bar_w * static_cast<int>(readings.size());
    if (chart_w > d.width()) chart_w = d.width();
    const int chart_left = (d.width() - chart_w) / 2;

    for (size_t i = 0; i < readings.size(); i++) {
        const auto &r = readings[i];
        int x = chart_left + static_cast<int>(i) * bar_w;
        if (x >= d.width()) break;
        int h = static_cast<int>(static_cast<float>(r.pkts) / static_cast<float>(max_pkts) * chart_h);
        if (h < 1 && r.pkts > 0) h = 1;
        int w = bar_w > 1 ? bar_w - 1 : 1;
        if (i == cursor) d.drawRect(x, chart_top, w, chart_h, kPaperPhosphor);
        /* Keep quiet channels visibly distinct from the black canvas. The
         * selected outline alone made an all-zero sweep look like one bar. */
        if (h > 0) d.fillRect(x, chart_bottom - h, w, h, barColour(r.pkts, max_pkts));
        else d.fillRect(x, chart_bottom - 2, w, 2, kDimGreen);
    }

    d.setTextColor(kMutedSlate, kVoidInk);
    d.setCursor(4, kBodyTop + 78);
    if (cursor < readings.size()) {
        const auto &r = readings[cursor];
        d.printf("CH %u %s  %u PKTS", r.ch, r.band5 ? "5G" : "2.4G", (unsigned)r.pkts);
    } else if (readings.empty()) {
        d.print(spectrum.active() ? "CHANNELS 0 · WAITING" : "NO CHANNEL READINGS");
    }
}

}  // namespace

void drawSpectrumView(const model::SpectrumModel &spectrum, size_t cursor, const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    if (spectrum.locked()) drawLocked(spectrum);
    else drawBroad(spectrum, cursor);

    endChrome(chrome);
}

}  // namespace ui
