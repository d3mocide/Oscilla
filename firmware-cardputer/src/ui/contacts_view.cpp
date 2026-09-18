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

constexpr int kSniffHeaderClientsX = 4;
constexpr int kSniffHeaderProbesX = 76;
constexpr int kSniffHeaderChannelX = 142;
constexpr int kSniffHeaderPacketsX = 184;
constexpr int kSniffClientChannelX = 104;
constexpr int kSniffClientBandX = 128;
constexpr int kSniffRssiX = 204;

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

        d.setCursor(6, y);
        d.setTextColor(kPaperPhosphor, bg);
        d.printf("%-15s", printable(r.mac, 15).c_str());
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
    d.printf("CH %u", contacts.currentChannel());
    d.setCursor(kSniffHeaderPacketsX, kBodyTop + 2);
    d.printf("PKTS %u", (unsigned)contacts.totalPkts());
    if (contacts.malformedRows()) {
        d.setTextColor(kSignalPink, kVoidInk);
        d.printf(" BAD %u", contacts.malformedRows());
    }

    if (clients) drawClientRows(contacts, cursor);
    else drawProbeRows(contacts, cursor);

    d.setCursor(4, kBodyTop + 83);
    if (!contacts.lastSighting().empty()) {
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print("LAST ");
        d.setTextColor(contacts.sniffing() ? kFieldGreen : kMutedSlate, kVoidInk);
        d.print(printable(contacts.lastSighting(), 32).c_str());
    } else {
        d.setTextColor(contacts.sniffing() ? kFieldGreen : kMutedSlate, kVoidInk);
        d.printf("CLIENTS %u · PROBES %u · PKTS %u",
                 (unsigned)contacts.clients().size(), (unsigned)contacts.probes().size(),
                 (unsigned)contacts.totalPkts());
    }

    endChrome(chrome);
}

}  // namespace ui
