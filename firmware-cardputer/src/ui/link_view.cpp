/*
 * link_view.cpp — see link_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/link_view.h"

#include <cstdio>

#include <M5Cardputer.h>

#include "storage/sd_storage.h"
#include "ui/canvas.h"
#include "ui/segmented_bar.h"
#include "ui/theme.h"

namespace ui {

std::string printable(const std::string &s, size_t max_len)
{
    std::string out;
    for (unsigned char c : s) {
        if (out.size() >= max_len) break;
        out += (c >= 0x20 && c <= 0x7E) ? static_cast<char>(c) : '?';
    }
    return out;
}

namespace {

uint16_t stateColour(ocp::LinkState s)
{
    switch (s) {
        case ocp::LinkState::Ready:        return kFieldGreen;
        case ocp::LinkState::HelloSent:    return kCalibrationYellow;
        case ocp::LinkState::Incompatible: return kSignalPink;
        case ocp::LinkState::Disconnected: return kFaultRed;
    }
    return kMutedSlate;
}

const char *linkStatus(ocp::LinkState state)
{
    switch (state) {
    case ocp::LinkState::Ready:        return "ACTIVE";
    case ocp::LinkState::HelloSent:    return "SYNCING";
    case ocp::LinkState::Incompatible: return "MISMATCH";
    case ocp::LinkState::Disconnected: return "OFFLINE";
    }
    return "UNKNOWN";
}

std::string capabilitySummary(const std::string &caps)
{
    std::string out;
    const auto add = [&out](const char *label) {
        if (!out.empty()) out += ' ';
        out += label;
    };
    if (caps.find("wifi24") != std::string::npos) add("2.4");
    if (caps.find("wifi5") != std::string::npos) add("5G");
    if (caps.find("ble") != std::string::npos) add("BLE");
    if (caps.find("ieee802154") != std::string::npos) add("154");
    if (caps.find("lora_rx") != std::string::npos) add("LORA");
    return out.empty() ? "--" : out;
}

void drawStatusPanel(M5Canvas &d, int y, const char *label, const char *value,
                     uint16_t color, bool selected)
{
    if (selected) {
        d.fillRect(0, y, d.width(), 20, kSelectionGlow);
        d.fillRect(0, y, 3, 20, kCalibrationYellow);
    }
    d.fillRect(5, y + 7, 5, 5, color);
    d.setCursor(16, y + 4);
    d.setTextColor(selected ? kPaperPhosphor : kMutedSlate, selected ? kSelectionGlow : kVoidInk);
    d.print(label);

    /* A compact health rail makes a live link read as a state, not a log row. */
    const int lit = color == kFieldGreen ? 4 : color == kCalibrationYellow ? 2 : 1;
    drawSegmentedBar(d, 113, y + 8, 4, 4, 4, 3, lit, color);

    d.setCursor(148, y + 4);
    d.setTextColor(color, selected ? kSelectionGlow : kVoidInk);
    d.print(value);
}

void drawCheckRow(M5Canvas &d, int y, const char *label, const char *value,
                  uint16_t color, bool selected)
{
    if (selected) {
        d.fillRect(0, y - 2, d.width(), 13, kSelectionGlow);
        d.fillRect(0, y - 2, 2, 13, kCalibrationYellow);
    }
    d.fillRect(6, y + 3, 4, 4, color);
    d.setCursor(16, y);
    d.setTextColor(selected ? kPaperPhosphor : kMutedSlate, selected ? kSelectionGlow : kVoidInk);
    d.print(label);
    d.setCursor(112, y);
    d.setTextColor(color, selected ? kSelectionGlow : kVoidInk);
    d.print(value);
}

}  // namespace

void drawLinkView(const ocp::Client &client, size_t cursor, const ChromeState &chrome)
{
    auto &d = ui::canvas();
    const auto &p = client.probe();
    const auto &st = client.stats();

    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(kMutedSlate, kVoidInk);
    if (client.state() == ocp::LinkState::Disconnected) {
        d.print("PROBE C5 / NOT PRESENT");
    } else {
        d.printf("PROBE C5 / %s", printable(p.ver, 10).c_str());
        d.setCursor(190, kBodyTop + 2);
        d.printf("P%d", p.proto);
    }

    drawStatusPanel(d, kBodyTop + 16, "PROBE LINK", linkStatus(client.state()),
                    stateColour(client.state()), cursor == 0);
    drawCheckRow(d, kBodyTop + 42, "GNSS", chrome.gnss == IndicatorState::Ready ? "FIXED" :
                                            chrome.gnss == IndicatorState::Fault ? "NO DATA" : "SEARCHING",
                 indicatorColour(chrome.gnss), cursor == 1);
    drawCheckRow(d, kBodyTop + 55, "SD CARD", storage::ready() ? "READY" : "ABSENT",
                 storage::ready() ? kFieldGreen : kFaultRed, cursor == 2);

    char bus[32];
    const bool bus_fault = st.errors || st.timeouts;
    const char *bus_state = bus_fault ? "FAULT" : client.state() == ocp::LinkState::Ready ? "OK" : "WAIT";
    std::snprintf(bus, sizeof bus, "%s R%u T%u E%u", bus_state,
                  st.resets, st.timeouts, st.errors);
    drawCheckRow(d, kBodyTop + 68, "BUS", bus,
                 bus_fault ? kSignalPink : client.state() == ocp::LinkState::Ready ? kFieldGreen : kCalibrationYellow,
                 cursor == 3);

    d.drawFastHLine(4, kBodyTop + 80, d.width() - 8, kLineBorder);
    d.setCursor(4, kBodyTop + 85);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print("CAPS");
    d.setCursor(38, kBodyTop + 85);
    if (client.state() == ocp::LinkState::Ready) {
        d.setTextColor(kFieldGreen, kVoidInk);
        d.print(capabilitySummary(p.caps).c_str());
    } else {
        d.setTextColor(kCalibrationYellow, kVoidInk);
        d.print("HANDSHAKE PENDING");
    }

    endChrome(chrome);
}

}  // namespace ui
