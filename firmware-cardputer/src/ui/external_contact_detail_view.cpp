/* Client-centred observed links (layout never claims distance or bearing); Observatory v2. */
#include "ui/external_contact_detail_view.h"
#include "ui/external_radar.h"
#include "ui/observatory_chrome.h"

namespace ui {
void drawExternalContactDetailStrip(lgfx::LGFX_Sprite &d, int top, const model::ContactsModel &m,
                                    const model::ContactInspection &p, uint32_t now)
{
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    const auto &key = p.key();
    const uint32_t age = now - p.sampleMs();
    const char *state = p.stateLabel(m.sniffing(), now);
    const bool live_present = p.present() && m.sniffing() && age < 5000;
    const BadgeTone tone = !p.active() ? BadgeTone::Cached
                         : live_present ? BadgeTone::Live
                         : !p.present() ? BadgeTone::Warn : BadgeTone::Cached;

    obsFrame(c, ModeSigil::NodeMesh, "CLIENT DOSSIER", state, tone);
    obsCard(c, 164, 36, 140, 103, p.present() ? kCalibrationYellow : kSignalPink);
    obsDock(c, live_present ? kFieldGreen : kCalibrationYellow, true);

    if (!p.active()) {
        obsMicro(c, 84, 84, "NO SNAPSHOT ROW", kCalibrationYellow, kVoidInk, Align::Center);
        obsMicro(c, 172, 42, "CLIENT DOSSIER", kMutedSlate, kPanelDark);
        obsText(c, 172, 60, "NO SELECTION", FontId::Title, kCalibrationYellow, 124);
        obsText(c, 12, 164, "SELECT A CLIENT", FontId::Title, kPaperPhosphor, 296);
        obsBay(c, 12, 94, "RSSI / dBm", "--", kCalibrationYellow);
        obsBay(c, 122, 86, "PACKETS", "--", kPaperPhosphor);
        obsBay(c, 224, 84, "AGE / s", "--", kMutedSlate);
        return;
    }

    /* Client-centred orbital graph (layout encodes identity only, never range). */
    constexpr int cx = 84, cy = 88;
    obsHexagon(c, cx, cy, 52, kTrackDark);
    obsDashedCircle(c, cx, cy, 44, kDimGreen, 32);
    c.ring(cx, cy, 22, kTrackDark);
    obsMicro(c, 16, 31, key.probes ? "REQUEST LINKS" : "OBSERVED AP LINKS", kDimGreen, kVoidInk);

    const auto graph = model::contactGraph(m, key, true);
    const size_t count = p.present() ? graph.size : 1;
    for (size_t i = count; i > 0; --i) {
        const auto &target = p.present()
                           ? (key.probes ? m.probes()[graph.indices[i - 1]].ssid : m.clients()[graph.indices[i - 1]].bssid)
                           : key.target;
        const auto node = orbitPoint(target, 38, cx, cy);
        const bool selected = target == key.target;
        if (p.present()) {
            c.line(cx, cy, node.x, node.y, selected ? kSignalPink : kDimGreen);
        } else {
            for (int step = 0; step < 8; step += 2) {
                c.line(cx + (node.x - cx) * step / 8, cy + (node.y - cy) * step / 8,
                       cx + (node.x - cx) * (step + 1) / 8, cy + (node.y - cy) * (step + 1) / 8, kMutedSlate);
            }
        }
        obsRadarNode(c, node, selected, !key.probes, !m.sniffing());
    }
    c.dot(cx, cy, 4, kPaperPhosphor);
    c.ring(cx, cy, 8, kCalibrationYellow);

    /* Right-side dossier card. */
    obsMicro(c, 172, 42, key.probes ? "SELECTED REQUEST" : "SELECTED AP", kMutedSlate, kPanelDark);
    obsText(c, 172, 55, key.target.empty() ? "<WILDCARD>" : key.target,
            FontId::Mono, kCalibrationYellow, 124);
    c.line(172, 72, 296, 72, kLineBorder);
    obsText(c, 172, 79,
            p.present() ? ("LINKS / " + std::to_string(graph.total)) : "ROW MISSING",
            FontId::Title, p.present() ? kPaperPhosphor : kSignalPink, 124);
    obsMicro(c, 172, 102,
             p.present() ? (std::to_string(graph.size) + "/" + std::to_string(graph.total) + " DRAWN")
                         : "RETAINED LAST OBS",
             kMutedSlate, kPanelDark);
    obsBadge(c, 172, 118, 106, 14, "LAYOUT / NO RANGE", kPanelDark, kLineBorder, kMutedSlate);

    /* Lower Reading Dock. */
    obsText(c, 12, 164, key.mac, FontId::Title, kPaperPhosphor, 296);
    if (!p.cachedApName().empty()) {
        obsText(c, 12, 184, "SCAN NAME / " + p.cachedApName(), FontId::Mono, kMutedSlate, 296);
    } else if (!key.probes) {
        obsMicro(c, 12, 185, "CH " + std::to_string(p.client().ch) + (p.client().band5 ? " / 5 GHz" : " / 2.4 GHz"),
                 kMutedSlate, kPanelDark);
    } else {
        obsMicro(c, 12, 185, "REQUEST / NOT ASSOCIATION", kMutedSlate, kPanelDark);
    }

    obsBay(c, 12, 94, "RSSI / dBm", std::to_string(p.rssi()), kCalibrationYellow);
    obsBay(c, 122, 86, "PACKETS", std::to_string(p.packets()), kPaperPhosphor);
    obsBay(c, 224, 84, "AGE / s", std::to_string(age / 1000), kMutedSlate, "s");
}
}  // namespace ui
