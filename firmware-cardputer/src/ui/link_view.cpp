/*
 * link_view.cpp — see link_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/link_view.h"

#include <M5Cardputer.h>

#include "storage/sd_storage.h"
#include "ui/canvas.h"

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
        case ocp::LinkState::Ready:        return TFT_GREEN;
        case ocp::LinkState::HelloSent:    return TFT_YELLOW;
        case ocp::LinkState::Incompatible: return TFT_MAGENTA;
        case ocp::LinkState::Disconnected: return TFT_RED;
    }
    return TFT_WHITE;
}

}  // namespace

void drawLinkView(const ocp::Client &client, const std::string &last_reply,
                  const std::string &notice, bool debug_mode)
{
    auto &d = ui::canvas();
    const auto &p = client.probe();
    const auto &st = client.stats();

    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(0, 0);

    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.print("OSCILLA deck  ");
    d.setTextColor(stateColour(client.state()), TFT_BLACK);
    d.print(ocp::linkStateName(client.state()));
    if (debug_mode) {
        d.setTextColor(TFT_ORANGE, TFT_BLACK);
        d.print("  DEBUG");
    }
    d.println();

    d.setTextColor(TFT_WHITE, TFT_BLACK);
    if (client.state() == ocp::LinkState::Disconnected) {
        d.println("probe: none");
    } else {
        d.printf("probe: %s %s  proto %d\n", printable(p.fw, 16).c_str(),
                 printable(p.ver, 12).c_str(), p.proto);
    }
    d.printf("caps:  %s\n", p.caps.empty() ? "(none)" : printable(p.caps, 30).c_str());
    d.print("sd:    ");
    d.setTextColor(storage::ready() ? TFT_GREEN : TFT_RED, TFT_BLACK);
    d.println(storage::ready() ? "ready" : "absent");
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.println();

    d.printf("resets %-4u timeouts %u\n", st.resets, st.timeouts);
    d.printf("noise  %-4u stray %-4u err %u\n", st.noise, st.stray, st.errors);
    d.printf("events %u\n", st.events);
    d.println();

    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.printf("last: %s\n", printable(last_reply, 32).c_str());
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.println(printable(notice, 38).c_str());

    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - 10);
    d.print(", / cards  w sweep  h/p/s system");
}

}  // namespace ui
