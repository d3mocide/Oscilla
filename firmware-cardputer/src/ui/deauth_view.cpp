/*
 * deauth_view.cpp — see deauth_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/deauth_view.h"

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

}  // namespace

void drawDeauthView(const model::DeauthModel &deauth, size_t cursor, const std::string &notice)
{
    auto &d = ui::canvas();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(0, 0);

    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.print("DEAUTH ");
    d.setTextColor(deauth.active() ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    d.print(deauth.active() ? "watching " : "stopped ");
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.printf("n=%u\n", (unsigned)deauth.totalCount());

    const auto &events = deauth.events();
    int list_top = kLineH + 2;
    int visible = (d.height() - list_top - 2 * kLineH) / kLineH;
    size_t first = cursor >= static_cast<size_t>(visible) ? cursor - visible + 1 : 0;

    for (int i = 0; i < visible && first + static_cast<size_t>(i) < events.size(); i++) {
        const auto &e = events[first + i];
        bool sel = first + static_cast<size_t>(i) == cursor;
        int y = list_top + i * kLineH;
        if (sel) d.fillRect(0, y - 1, d.width(), kLineH, TFT_NAVY);
        uint16_t bg = sel ? TFT_NAVY : TFT_BLACK;

        /* Colour carries deauth-vs-disassoc so the text stays this compact. */
        d.setCursor(0, y);
        d.setTextColor(e.disassoc ? TFT_YELLOW : TFT_ORANGE, bg);
        d.printf("%-17s", printable(e.mac, 17).c_str());
        d.setTextColor(TFT_LIGHTGREY, bg);
        d.printf(" r%-3ld", e.reason);
        d.setTextColor(rssiColour(e.rssi), bg);
        d.printf(" %4d", e.rssi);
    }

    if (events.empty()) {
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.setCursor(0, list_top);
        d.print(deauth.active() ? "listening..." : "no detections yet");
    }

    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(0, d.height() - 2 * kLineH);
    d.print(printable(notice, 38).c_str());
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - kLineH);
    d.print(";. move  s start/stop  ` back");
}

}  // namespace ui
