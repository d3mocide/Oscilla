/*
 * bt_view.cpp — see bt_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/bt_view.h"

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

void drawDeviceRows(const model::BtModel &bt, size_t cursor, int list_top, int visible)
{
    auto &d = ui::canvas();
    const auto &rows = bt.devices();
    size_t first = cursor >= (size_t)visible ? cursor - visible + 1 : 0;

    for (int i = 0; i < visible && first + i < rows.size(); i++) {
        const auto &r = rows[first + i];
        bool sel = first + i == cursor;
        int y = list_top + i * kLineH;
        if (sel) d.fillRect(0, y - 1, d.width(), kLineH, TFT_NAVY);
        uint16_t bg = sel ? TFT_NAVY : TFT_BLACK;

        std::string label = r.name.empty() ? r.mac : printable(r.name, 17);
        d.setCursor(0, y);
        d.setTextColor(r.tracker ? TFT_ORANGE : (r.name.empty() ? TFT_DARKGREY : TFT_WHITE), bg);
        d.printf("%-17s", label.c_str());
        d.setTextColor(TFT_LIGHTGREY, bg);
        d.printf(" %s ", r.tracker ? "T" : " ");
        d.setTextColor(rssiColour(r.rssi), bg);
        d.printf("%4d", r.rssi);
    }
}

}  // namespace

void drawBtView(const model::BtModel &bt, size_t cursor, const std::string &notice)
{
    auto &d = ui::canvas();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(0, 0);

    /* scan_bt, start_ble_scan and scan_airtag all hold PHY_OWNER_BLE
     * exclusively (radio_arbiter, DESIGN §6.2), so at most one of these
     * three is ever true at once — one status word covers all of it. */
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.print("BEACONS ");
    if (bt.scanning()) { d.setTextColor(TFT_GREEN, TFT_BLACK); d.println("scanning"); }
    else if (bt.continuousActive()) { d.setTextColor(TFT_GREEN, TFT_BLACK); d.println("live"); }
    else if (bt.airtagActive()) { d.setTextColor(TFT_ORANGE, TFT_BLACK); d.println("airtag-live"); }
    else { d.setTextColor(TFT_DARKGREY, TFT_BLACK); d.println("idle"); }

    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.printf("[devices %u]", (unsigned)bt.devices().size());
    d.setTextColor(bt.trackerCount() ? TFT_ORANGE : TFT_DARKGREY, TFT_BLACK);
    d.printf(" [trackers %u]", (unsigned)bt.trackerCount());
    if (bt.malformedRows()) {
        d.setTextColor(TFT_ORANGE, TFT_BLACK);
        d.printf("  %u bad", bt.malformedRows());
    }

    int list_top = 2 * kLineH + 2;
    int visible = (d.height() - list_top - 3 * kLineH) / kLineH;
    drawDeviceRows(bt, cursor, list_top, visible);

    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - 3 * kLineH);
    if (!bt.trackerHits().empty()) {
        const auto &last = bt.trackerHits()[0];
        d.printf("last tracker: %s %d", printable(last.mac, 17).c_str(), last.rssi);
    }
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(0, d.height() - 2 * kLineH);
    d.print(printable(notice, 38).c_str());
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - kLineH);
    d.print(";. move  ,/ cards  s scan  c live  a airtag");
}

}  // namespace ui
