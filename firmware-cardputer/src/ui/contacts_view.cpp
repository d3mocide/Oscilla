/*
 * contacts_view.cpp — see contacts_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/contacts_view.h"

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

void drawClientRows(const model::ContactsModel &contacts, size_t cursor, int list_top, int visible)
{
    auto &d = ui::canvas();
    const auto &rows = contacts.clients();
    size_t first = cursor >= (size_t)visible ? cursor - visible + 1 : 0;

    for (int i = 0; i < visible && first + i < rows.size(); i++) {
        const auto &r = rows[first + i];
        bool sel = first + i == cursor;
        int y = list_top + i * kLineH;
        if (sel) d.fillRect(0, y - 1, d.width(), kLineH, TFT_NAVY);
        uint16_t bg = sel ? TFT_NAVY : TFT_BLACK;

        d.setCursor(0, y);
        d.setTextColor(TFT_WHITE, bg);
        d.printf("%-17s", printable(r.mac, 17).c_str());
        d.setTextColor(TFT_LIGHTGREY, bg);
        d.printf(" %3u %-2s ", r.ch, r.band5 ? "5G" : "2G");
        d.setTextColor(rssiColour(r.rssi), bg);
        d.printf("%4d", r.rssi);
    }
}

void drawProbeRows(const model::ContactsModel &contacts, size_t cursor, int list_top, int visible)
{
    auto &d = ui::canvas();
    const auto &rows = contacts.probes();
    size_t first = cursor >= (size_t)visible ? cursor - visible + 1 : 0;

    for (int i = 0; i < visible && first + i < rows.size(); i++) {
        const auto &r = rows[first + i];
        bool sel = first + i == cursor;
        int y = list_top + i * kLineH;
        if (sel) d.fillRect(0, y - 1, d.width(), kLineH, TFT_NAVY);
        uint16_t bg = sel ? TFT_NAVY : TFT_BLACK;

        std::string ssid = r.ssid.empty() ? "<wildcard>" : printable(r.ssid, 22);
        d.setCursor(0, y);
        d.setTextColor(r.ssid.empty() ? TFT_DARKGREY : TFT_WHITE, bg);
        d.printf("%-22s", ssid.c_str());
        d.setTextColor(rssiColour(r.rssi), bg);
        d.printf("%4d", r.rssi);
    }
}

}  // namespace

void drawContactsView(const model::ContactsModel &contacts, ContactsTab tab, size_t cursor,
                      const std::string &notice)
{
    auto &d = ui::canvas();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(0, 0);

    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.print("CONTACTS ");
    d.setTextColor(contacts.sniffing() ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
    d.print(contacts.sniffing() ? "live " : "stopped ");
    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.printf("ch=%u pkts=%u\n", contacts.currentChannel(), (unsigned)contacts.totalPkts());

    bool clients = tab == ContactsTab::Clients;
    d.setTextColor(clients ? TFT_WHITE : TFT_DARKGREY, TFT_BLACK);
    d.printf("[clients %u]", (unsigned)contacts.clients().size());
    d.setTextColor(clients ? TFT_DARKGREY : TFT_WHITE, TFT_BLACK);
    d.printf(" [probes %u]", (unsigned)contacts.probes().size());
    if (contacts.malformedRows()) {
        d.setTextColor(TFT_ORANGE, TFT_BLACK);
        d.printf("  %u bad", contacts.malformedRows());
    }

    int list_top = 2 * kLineH + 2;
    int visible = (d.height() - list_top - 3 * kLineH) / kLineH;
    if (clients) drawClientRows(contacts, cursor, list_top, visible);
    else drawProbeRows(contacts, cursor, list_top, visible);

    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - 3 * kLineH);
    d.print(printable(contacts.lastSighting(), 38).c_str());
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(0, d.height() - 2 * kLineH);
    d.print(printable(notice, 38).c_str());
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - kLineH);
    d.print(";. move  ,/ cards  x tab  s start/stop");
}

}  // namespace ui
