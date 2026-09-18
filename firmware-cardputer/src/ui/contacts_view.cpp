/*
 * contacts_view.cpp — see contacts_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/contacts_view.h"

#include <M5Cardputer.h>

#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/theme.h"

namespace ui {

namespace {

uint16_t rssiColour(int rssi)
{
    if (rssi >= -55) return kFieldGreen;
    if (rssi >= -70) return kCalibrationYellow;
    return kFaultRed;
}

void drawClientRows(const model::ContactsModel &contacts, size_t cursor)
{
    auto &d = ui::canvas();
    const auto &rows = contacts.clients();
    constexpr int list_top = kBodyTop + 16;
    constexpr int row_height = 13;
    constexpr int visible = 4;
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

        d.setCursor(0, y);
        d.setTextColor(kPaperPhosphor, bg);
        d.printf("%c%-15s", sel ? '>' : ' ', printable(r.mac, 15).c_str());
        d.setTextColor(kMutedSlate, bg);
        d.printf(" %3u %-4s", r.ch, r.band5 ? "5G" : "2.4G");
        d.setTextColor(rssiColour(r.rssi), bg);
        d.printf(" %4d", r.rssi);
    }
}

void drawProbeRows(const model::ContactsModel &contacts, size_t cursor)
{
    auto &d = ui::canvas();
    const auto &rows = contacts.probes();
    constexpr int list_top = kBodyTop + 16;
    constexpr int row_height = 13;
    constexpr int visible = 4;
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

        std::string ssid = r.ssid.empty() ? "<wildcard>" : printable(r.ssid, 22);
        d.setCursor(0, y);
        d.setTextColor(r.ssid.empty() ? kDimGreen : kPaperPhosphor, bg);
        d.printf("%c%-21s", sel ? '>' : ' ', ssid.c_str());
        d.setTextColor(rssiColour(r.rssi), bg);
        d.printf(" %4d", r.rssi);
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
    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.printf("%s %u  PROBES %u  CH %u  PKTS %u", clients ? "CLIENTS" : "PROBES",
             clients ? (unsigned)contacts.clients().size() : (unsigned)contacts.probes().size(),
             (unsigned)contacts.probes().size(), contacts.currentChannel(),
             (unsigned)contacts.totalPkts());
    if (contacts.malformedRows()) {
        d.setTextColor(kSignalPink, kVoidInk);
        d.printf(" BAD %u", contacts.malformedRows());
    }

    if (clients) drawClientRows(contacts, cursor);
    else drawProbeRows(contacts, cursor);

    d.setCursor(4, kBodyTop + 83);
    if (!contacts.lastSighting().empty()) {
        d.setTextColor(contacts.sniffing() ? kFieldGreen : kMutedSlate, kVoidInk);
        d.print(printable(contacts.lastSighting(), 38).c_str());
    } else {
        d.setTextColor(contacts.sniffing() ? kFieldGreen : kMutedSlate, kVoidInk);
        d.printf("CLIENTS %u · PROBES %u · PKTS %u",
                 (unsigned)contacts.clients().size(), (unsigned)contacts.probes().size(),
                 (unsigned)contacts.totalPkts());
    }

    endChrome(chrome);
}

}  // namespace ui
