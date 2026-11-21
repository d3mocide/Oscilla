/*
 * contacts_view.cpp — see contacts_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/contacts_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {

namespace {

constexpr int kSniffHeaderClientsX = 4;
constexpr int kSniffHeaderProbesX = 76;
constexpr int kSniffHeaderChannelX = 142;
constexpr int kSniffClientChannelX = 118;
constexpr int kSniffClientBandX = 145;
constexpr int kSniffRssiX = 204;

void drawClientRows(const model::ContactsModel &contacts, size_t cursor)
{
    auto &d = ui::canvas();
    const auto &rows = contacts.clients();
    size_t first = listFirstVisible(cursor);

    for (int i = 0; i < kListVisibleRows && first + i < rows.size(); i++) {
        const auto &r = rows[first + i];
        bool sel = first + i == cursor;
        int y = kListTop + i * kListRowHeight;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);

        d.setCursor(6, y);
        d.setTextColor(kPaperPhosphor, bg);
        d.printf("%-17s", printable(r.mac, 17).c_str());
        d.setTextColor(kMutedSlate, bg);
        d.setCursor(kSniffClientChannelX, y);
        d.printf("%3u", r.ch);
        d.setCursor(kSniffClientBandX, y);
        d.printf("%4s", r.band5 ? "5G" : "2.4G");
        d.setTextColor(rssiColour(r.rssi), bg);
        d.setCursor(kSniffRssiX, y);
        d.printf("%4d", r.rssi);
    }
}

void drawProbeRows(const model::ContactsModel &contacts, size_t cursor)
{
    auto &d = ui::canvas();
    const auto &rows = contacts.probes();
    size_t first = listFirstVisible(cursor);

    for (int i = 0; i < kListVisibleRows && first + i < rows.size(); i++) {
        const auto &r = rows[first + i];
        bool sel = first + i == cursor;
        int y = kListTop + i * kListRowHeight;
        uint16_t bg = sel ? kSelectionGlow : kVoidInk;
        drawRowHighlight(d, y, sel);

        std::string ssid = r.ssid.empty() ? "<wildcard>" : printable(r.ssid, 22);
        d.setCursor(6, y);
        d.setTextColor(r.ssid.empty() ? kDimGreen : kPaperPhosphor, bg);
        d.printf("%-21s", ssid.c_str());
        d.setTextColor(rssiColour(r.rssi), bg);
        d.setCursor(kSniffRssiX, y);
        d.printf("%4d", r.rssi);
    }
}

}  // namespace

void drawContactsView(const model::ContactsModel &contacts, ContactsTab tab, size_t cursor,
                      const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    const bool clients = tab == ContactsTab::Clients;
    d.setCursor(kSniffHeaderClientsX, kBodyTop + 2);
    d.setTextColor(clients ? kPaperPhosphor : kMutedSlate, kVoidInk);
    d.printf("CLIENTS %u", (unsigned)contacts.clients().size());
    d.setCursor(kSniffHeaderProbesX, kBodyTop + 2);
    d.setTextColor(clients ? kMutedSlate : kPaperPhosphor, kVoidInk);
    d.printf("PROBES %u", (unsigned)contacts.probes().size());
    d.setCursor(kSniffHeaderChannelX, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    if(contacts.currentChannel())d.printf("CH %u",contacts.currentChannel());
    else d.print("CH --");

    if (clients) drawClientRows(contacts, cursor);
    else drawProbeRows(contacts, cursor);

    if ((clients && contacts.clients().empty()) || (!clients && contacts.probes().empty())) {
        d.setCursor(4, kListTop);
        d.setTextColor(kDimGreen, kVoidInk);
        d.print(contacts.sniffing() ? "LISTENING FOR WIFI FRAMES..." : "NO OBSERVATIONS YET");
    }

    d.setCursor(4, kDetailRowY);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print("LAST ");
    d.setTextColor(contacts.sniffing() ? kFieldGreen : kMutedSlate, kVoidInk);
    d.print(printable(contacts.lastSighting().empty() ? "NO SIGHTINGS" : contacts.lastSighting(), 32).c_str());
    d.setCursor(4, kHintRowY);
    d.setTextColor(kCalibrationYellow, kVoidInk);
    d.print(clients ? "ENTER DETAIL / x REQUESTS" : "ENTER DETAIL / x CLIENTS");

    endChrome(chrome);
}

}  // namespace ui
