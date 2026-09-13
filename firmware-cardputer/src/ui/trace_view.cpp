/*
 * trace_view.cpp — see trace_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/trace_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"

namespace ui {

namespace {

constexpr int kLineH = 10;

void label(const char *text)
{
    auto &d = ui::canvas();
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.printf("%-8s", text);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
}

}  // namespace

void drawTraceView(const model::ApRow *row, const model::Inspect &in, bool listening,
                   const std::string &notice)
{
    auto &d = ui::canvas();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(0, 0);

    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.println("TRACE");

    if (!row) {
        d.setTextColor(TFT_YELLOW, TFT_BLACK);
        d.println("list cleared - rescan");
    } else {
        label("ssid");   d.println(row->ssid.empty() ? "<hidden>" : printable(row->ssid, 30).c_str());
        label("bssid");  d.println(printable(row->bssid, 17).c_str());
        label("chan");   d.printf("%u  (%s GHz)\n", row->ch, row->band5 ? "5" : "2.4");
        label("auth");   d.println(printable(row->auth, 20).c_str());

        if (listening) {
            d.setTextColor(TFT_YELLOW, TFT_BLACK);
            d.println("\nlistening for beacons...");
        } else if (!in.valid) {
            d.setTextColor(TFT_DARKGREY, TFT_BLACK);
            d.println("\npress i to inspect");
        } else if (in.beacons == 0) {
            d.setTextColor(TFT_ORANGE, TFT_BLACK);
            d.println(in.aborted ? "\ninspect stopped" : "\nno beacons heard");
        } else {
            label("rssi");   d.printf("%d dBm  (%u beacons)\n", in.rssi, in.beacons);
            label("mfp");
            d.setTextColor(in.mfp_required ? TFT_GREEN : in.mfp_capable ? TFT_YELLOW : TFT_ORANGE, TFT_BLACK);
            d.println(!in.rsn ? "no RSN" : in.mfp_required ? "required" : in.mfp_capable ? "capable" : "off");
            label("uptime");
            uint64_t s = in.uptime_s;
            d.printf("%llud %lluh %llum  (TSF)\n", s / 86400, (s % 86400) / 3600, (s % 3600) / 60);
            label("beacon"); d.printf("%u ms\n", in.interval_ms);
        }
    }

    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(0, d.height() - 2 * kLineH);
    d.print(printable(notice, 38).c_str());
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - kLineH);
    d.print("i inspect again  ` back");
}

}  // namespace ui
