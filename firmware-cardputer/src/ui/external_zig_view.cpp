/* Passive 802.15.4 MAC inventory with multi-item LQI field (never topology); Observatory v2. */
#include "ui/external_zig_view.h"
#include "ui/observatory_chrome.h"
#include <cstdio>

namespace ui {
void drawExternalZigStrip(lgfx::LGFX_Sprite &d, int top, const model::ZigModel &m, bool pans, size_t cursor)
{
    using namespace instrument;
    using namespace observatory;
    const size_t total = pans ? m.pans().size() : m.nodes().size();
    const bool live = m.active();
    Canvas c(d, top);

    obsFrame(c, ModeSigil::PulseBars,
             pans ? "802.15.4 // PAN FIELD" : "802.15.4 // NODE FIELD",
             live ? "LIVE" : "CACHED",
             live ? BadgeTone::Live : BadgeTone::Cached);
    obsDock(c, live ? kFieldGreen : kMutedSlate, true);

    constexpr int x0 = 36, y0 = 46, w = 264, h = 76;
    for (int i = 0; i <= 4; ++i) {
        const int yy = y0 + (h * i) / 4;
        if (i == 4) c.line(x0, yy, x0 + w, yy, kDimGreen);
        else obsDashedH(c, x0, x0 + w, yy, kTrackDark, 2, 3);
    }
    obsMicro(c, 32, y0 - 3, "255", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, 32, y0 + h / 2 - 3, "128", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, 32, y0 + h - 4, "0", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, x0, 32, "LINK QUALITY (LQI)", kDimGreen, kVoidInk);
    obsMicro(c, x0 + w, 32, "NO TOPOLOGY INFERRED", kMutedSlate, kVoidInk, Align::Right);

    if (cursor >= total) {
        obsMicro(c, 160, 82, "NO MAC INVENTORY OBSERVATIONS", kCalibrationYellow, kVoidInk, Align::Center);
        obsText(c, 12, 164, pans ? "PAN INVENTORY" : "NODE INVENTORY", FontId::Title, kPaperPhosphor, 296);
        obsMicro(c, 12, 185, "PASSIVE 802.15.4 MAC INVENTORY", kMutedSlate, kPanelDark);
        obsBay(c, 12, 94, "LQI", "--", kCalibrationYellow);
        obsBay(c, 122, 86, "RSSI / dBm", "--", kPaperPhosphor);
        obsBay(c, 224, 84, pans ? "NODES" : "SEEN", "--", kMutedSlate);
        return;
    }

    /* Multi-item LQI column field (up to 12 visible PANs/nodes, selected framed in pink/yellow). */
    const size_t first = firstVisible(total, cursor, 12);
    const size_t visible = std::min(size_t(12), total - first);
    const int step = std::min(36, w / int(visible));
    const int field_w = step * int(visible);
    const int start_x = x0 + (w - field_w) / 2;
    const int bw = std::max(8, step - 6);

    for (size_t i = 0; i < visible; ++i) {
        const size_t idx = first + i;
        const bool sel = idx == cursor;
        const unsigned lqi = pans ? m.pans()[idx].lqi : m.nodes()[idx].lqi;
        const int x = start_x + int(i) * step + 3;
        const int bh = std::max(2, int(lqi * h / 255));
        c.rect(x, y0 + h - bh, bw, bh, sel ? kDimGreen : kTrackDark);
        c.line(x, y0 + h - bh, x + bw - 1, y0 + h - bh,
               sel ? kCalibrationYellow : live ? kFieldGreen : kMutedSlate);
        for (int yy = y0 + h - bh + 5; yy < y0 + h; yy += 6) {
            c.line(x, yy, x + bw - 1, yy, live ? kDimGreen : kLineBorder);
        }
        if (sel) c.corners(x - 2, y0 - 2, bw + 4, h + 4, kSignalPink);
        std::string lbl = pans ? m.pans()[idx].pan : m.nodes()[idx].short_addr;
        if (lbl.size() > 4) lbl = lbl.substr(lbl.size() - 4);
        obsMicro(c, x + bw / 2, 128, obsUpper(lbl), sel ? kCalibrationYellow : kMutedSlate, kVoidInk, Align::Center);
    }

    const unsigned lqi = pans ? m.pans()[cursor].lqi : m.nodes()[cursor].lqi;
    const int rssi = pans ? m.pans()[cursor].rssi : m.nodes()[cursor].rssi;
    const std::string id = pans ? ("PAN " + obsUpper(m.pans()[cursor].pan))
                                : (obsUpper(m.nodes()[cursor].pan) + " / " + obsUpper(m.nodes()[cursor].short_addr));
    obsText(c, 12, 164, id, FontId::Title, kPaperPhosphor, 210);
    obsMicro(c, 308, 168, std::to_string(total) + (pans ? " PANS" : " NODES"), kMutedSlate, kPanelDark, Align::Right);

    if (pans && !m.pans()[cursor].channels.empty()) {
        obsMicro(c, 12, 185, "CH " + m.pans()[cursor].channels, kMutedSlate, kPanelDark);
    } else if (!pans && !m.nodes()[cursor].ext.empty()) {
        obsText(c, 12, 184, "EXT " + m.nodes()[cursor].ext, FontId::Mono, kMutedSlate, 185);
    } else {
        obsMicro(c, 12, 185, "PASSIVE MAC INVENTORY", kMutedSlate, kPanelDark);
    }
    obsMicro(c, 308, 185, "NO TOPOLOGY", kMutedSlate, kPanelDark, Align::Right);

    obsBay(c, 12, 94, "LQI / 255", std::to_string(lqi), kCalibrationYellow);
    obsBay(c, 122, 86, "RSSI / dBm", std::to_string(rssi), kPaperPhosphor);
    obsBay(c, 224, 84, pans ? "NODES" : "SEEN",
           std::to_string(pans ? m.pans()[cursor].nodes : m.nodes()[cursor].seen), kFieldGreen);
}
}  // namespace ui
