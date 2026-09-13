/*
 * sweep_view.cpp — see sweep_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/sweep_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"

namespace ui {

namespace {

constexpr int kLineH = 10;

uint16_t rssiColour(int rssi)
{
    if (rssi >= -55) return TFT_GREEN;
    if (rssi >= -70) return TFT_YELLOW;
    return TFT_ORANGE;
}

/* Short security label so the row fits 40 columns. */
const char *shortAuth(const std::string &a)
{
    if (a == "WPA2/WPA3") return "W2/3";
    if (a == "WPA/WPA2") return "W1/2";
    if (a == "WPA2-EAP" || a == "WPA-EAP" || a == "WPA3-EAP" || a == "WPA2/WPA3-EAP" || a == "WPA3-EAP192") return "EAP";
    if (a == "OPEN") return "open";
    return a.c_str();
}

}  // namespace

void drawSweepView(const model::ScanModel &scan, size_t cursor, uint32_t scanning_ms,
                   const std::string &notice)
{
    auto &d = ui::canvas();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(0, 0);

    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.print("SWEEP  ");
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    if (scan.scanning()) {
        d.printf("listening... %us", (unsigned)(scanning_ms / 1000));
    } else if (scan.aborted()) {
        d.print("stopped");
    } else {
        d.printf("%u APs  %.1fs", (unsigned)scan.rows().size(), scan.elapsedMs() / 1000.0);
        if (scan.malformedRows()) d.printf("  %u bad", scan.malformedRows());
    }

    const auto &rows = scan.rows();
    int list_top = kLineH + 2;
    int visible = (d.height() - list_top - 2 * kLineH) / kLineH;
    size_t first = cursor >= (size_t)visible ? cursor - visible + 1 : 0;

    for (int i = 0; i < visible && first + i < rows.size(); i++) {
        const auto &r = rows[first + i];
        bool sel = first + i == cursor;
        int y = list_top + i * kLineH;
        if (sel) d.fillRect(0, y - 1, d.width(), kLineH, TFT_NAVY);
        uint16_t bg = sel ? TFT_NAVY : TFT_BLACK;

        std::string name = r.ssid.empty() ? "<hidden>" : printable(r.ssid, 15);
        d.setCursor(0, y);
        d.setTextColor(r.ssid.empty() ? TFT_DARKGREY : TFT_WHITE, bg);
        d.printf("%-15s", name.c_str());
        d.setTextColor(TFT_LIGHTGREY, bg);
        d.printf(" %3u %-2s %-4s ", r.ch, r.band5 ? "5G" : "2G", shortAuth(r.auth));
        d.setTextColor(rssiColour(r.rssi), bg);
        d.printf("%4d", r.rssi);
    }

    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(0, d.height() - 2 * kLineH);
    d.print(printable(notice, 38).c_str());
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - kLineH);
    d.print(";. move  enter trace  r rescan  ` back");
}

}  // namespace ui
