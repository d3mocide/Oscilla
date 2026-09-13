/*
 * spectrum_view.cpp — see spectrum_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/spectrum_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"

namespace ui {

namespace {

constexpr int kLineH = 10;

uint16_t barColour(uint32_t pkts, uint32_t max_pkts)
{
    if (max_pkts == 0) return TFT_DARKGREY;
    float frac = static_cast<float>(pkts) / static_cast<float>(max_pkts);
    if (frac > 0.66f) return TFT_RED;
    if (frac > 0.33f) return TFT_YELLOW;
    return TFT_GREEN;
}

void drawLocked(const model::SpectrumModel &spectrum)
{
    auto &d = ui::canvas();
    uint32_t pkts = spectrum.readings().empty() ? 0 : spectrum.readings()[0].pkts;

    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.printf("channel %u\n\n", spectrum.lockedChannel());
    d.setTextSize(3);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.printf("%u\n", (unsigned)pkts);
    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.println("packets/sec");
}

void drawBroad(const model::SpectrumModel &spectrum, size_t cursor)
{
    auto &d = ui::canvas();
    const auto &readings = spectrum.readings();

    uint32_t max_pkts = 1;
    for (const auto &r : readings) if (r.pkts > max_pkts) max_pkts = r.pkts;

    int chart_top = 2 * kLineH;
    int chart_bottom = d.height() - 3 * kLineH;
    int chart_h = chart_bottom - chart_top;
    int bar_w = readings.empty() ? 1 : d.width() / static_cast<int>(readings.size());
    if (bar_w > 14) bar_w = 14;
    if (bar_w < 1) bar_w = 1;

    for (size_t i = 0; i < readings.size(); i++) {
        const auto &r = readings[i];
        int x = static_cast<int>(i) * bar_w;
        if (x >= d.width()) break;
        int h = static_cast<int>(static_cast<float>(r.pkts) / static_cast<float>(max_pkts) * chart_h);
        if (h < 1 && r.pkts > 0) h = 1;
        int w = bar_w > 1 ? bar_w - 1 : 1;
        if (i == cursor) d.drawRect(x, chart_top, w, chart_h, TFT_WHITE);
        d.fillRect(x, chart_bottom - h, w, h, barColour(r.pkts, max_pkts));
    }

    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.setCursor(0, chart_bottom + 2);
    if (cursor < readings.size()) {
        const auto &r = readings[cursor];
        d.printf("ch %u (%s)  %u pkts", r.ch, r.band5 ? "5G" : "2G", (unsigned)r.pkts);
    } else if (readings.empty()) {
        d.print("listening...");
    }
}

}  // namespace

void drawSpectrumView(const model::SpectrumModel &spectrum, size_t cursor, const std::string &notice)
{
    auto &d = ui::canvas();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(0, 0);

    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.print("SPECTRUM ");
    d.setTextColor(spectrum.active() ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    d.println(!spectrum.active() ? "stopped" : spectrum.locked() ? "locked" : "scanning");

    if (spectrum.locked()) drawLocked(spectrum);
    else drawBroad(spectrum, cursor);

    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(0, d.height() - 2 * kLineH);
    d.print(printable(notice, 38).c_str());
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - kLineH);
    d.print(";. select  enter lock  s scan  ` back");
}

}  // namespace ui
