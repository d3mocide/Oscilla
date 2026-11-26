/*
 * help_view.cpp — the shared Cardputer keyboard reference.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/help_view.h"

#include "ui/canvas.h"
#include "ui/theme.h"

namespace ui {

namespace {

struct Controls {
    const char *title;
    const char *lines[5];
};

Controls controlsFor(app::Screen screen, HelpDetail detail)
{
    using app::Screen;
    switch (detail) {
    case HelpDetail::Hunt:
        return {"SIGNAL HUNT", {"a       Toggle audio cues", "r       Reset signal peak", "`       Exit Hunt", "Faster ticks mean stronger signal"}};
    case HelpDetail::BleDossier:
        return {"BLE DOSSIER", {"r / s   Refresh selected device", "f       Hunt selected device", "`       Return to BLE Scan"}};
    case HelpDetail::ContactDossier:
        return {"CONTACT DOSSIER", {"s       Start / stop reception", "`       Return to Sniffer"}};
    case HelpDetail::LoraSpectrum:
        return {"LORA SPECTRUM", {"s       Start / stop sweep", "x       Next frequency preset", "r       Clear peak hold", "w       Switch to packet mode"}};
    case HelpDetail::LoraPacket:
        return {"LORA PACKET", {"; .     Previous / next hex page", "s       Start / stop LoRa RX", "`       Return to LoRa RX"}};
    case HelpDetail::Channel:
        return {"PACKET MONITOR / CHANNEL", {"s       Start / stop channel RX", "`       Return to channel survey"}};
    case HelpDetail::SessionSummary:
        return {"SESSION SUMMARY", {"Enter   Open saved records", "`       Return to session list"}};
    case HelpDetail::SessionRecords:
        return {"SESSION RECORDS", {"; .     Previous / next record", "Enter   Open LoRa packet hex", "`       Return to summary"}};
    case HelpDetail::SessionPacket:
        return {"SAVED PACKET", {"; .     Previous / next hex page", "`       Return to records"}};
    case HelpDetail::None:
        break;
    }
    switch (screen) {
    case Screen::Link:
        return {"LINK", {"`       Open Settings", ", /     Previous / next card", "; .     Select row; Enter acts", "w       Run a Wi-Fi scan", "p / s   Ping / request status"}};
    case Screen::Info:
        return {"SYSTEM", {"i / `   Return to originating view", ", /     Previous / next card", "o       Open saved sessions"}};
    case Screen::Settings:
        return {"SETTINGS", {"; .     Previous / next setting", "Enter   Change selected setting", ", /     Adjust down / up", "`       Return to Link"}};
    case Screen::Sweep:
        return {"WIFI SCAN", {"f       Hunt the selected AP", "; .     Select AP; Enter for detail", "r       Run a snapshot scan", "c       Start / stop live discovery", "b       Save / clear baseline"}};
    case Screen::Trace:
        return {"AP DOSSIER", {"f       Hunt this AP", "r       Refresh beacon details", "x       Toggle radio / clients", "; .     Select observed client", "`       Return to Wi-Fi Scan"}};
    case Screen::Beacons:
        return {"BLE SCAN", {"f       Hunt the selected device", "; .     Select device; Enter for info", "s       Run a snapshot scan", "c       Start / stop live discovery", "a       Toggle tracker watch"}};
    case Screen::Contacts:
        return {"SNIFFER", {"s       Start / stop reception", "x       Toggle clients / probes", "; .     Select row; Enter for detail"}};
    case Screen::Spectrum:
        return {"PACKET MONITOR", {"s       Start / stop channel survey", "; .     Select a channel", "Enter   Monitor selected channel"}};
    case Screen::Zig:
        return {"802.15.4", {"s       Start / stop reception", "x       Toggle PANs / nodes", "; .     Previous / next row"}};
    case Screen::SubGhz:
        return {"LORA RX", {"x / c   Select / apply RX profile", "s       Start / stop LoRa RX", "w       Switch to spectrum mode", "m       Save waypoint with GNSS fix", "; .     Select packet; Enter for hex"}};
    case Screen::Ook:
        return {"OOK RX", {"s       Start / stop 433 MHz RX", "; .     Select a transmitter"}};
    case Screen::Deauth:
        return {"DEAUTH DETECT", {"s       Start / stop passive watch", "; .     Previous / next event"}};
    case Screen::AntiSurveillance:
        return {"ANTI-SURV", {"s       Start / stop passive watch", "; .     Select a tracker", "f       Hunt the selected tracker"}};
    case Screen::Gnss:
        return {"GNSS", {"Enter   Open Wardrive", ", /     Previous / next card", "External panel shows the fix reticle"}};
    case Screen::Drive:
        return {"WARDRIVE", {"l       Start / stop SD logging", "m       Save waypoint with GNSS fix", "o       Open saved sessions"}};
    case Screen::Logs:
        return {"SESSIONS", {"; .     Select session", "Enter   Open summary", "x       Cycle file filters", "r       Refresh session list", "i       Open System"}};
    }
    return {"CONTROLS", {}};
}

}  // namespace

void drawHelpView(const ChromeState &chrome, app::Screen screen, HelpDetail detail)
{
    auto &d = canvas();
    const auto controls = controlsFor(screen, detail);
    beginChrome(chrome);
    d.setTextSize(1);

    d.setCursor(4, kBodyTop + 3);
    d.setTextColor(kFieldGreen, kVoidInk);
    d.print(controls.title);
    d.drawFastHLine(4, kBodyTop + 14, 232, kDimGreen);

    d.setTextColor(kPaperPhosphor, kVoidInk);
    int y = kBodyTop + 20;
    for (const char *line : controls.lines) {
        if (!line) break;
        d.setCursor(4, y);
        d.print(line);
        y += 12;
    }

    endChrome(chrome, true);
}

}  // namespace ui
