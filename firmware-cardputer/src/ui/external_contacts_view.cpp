/* Selected AP or probe-request clusters; Observatory v2. */
#include "ui/external_contacts_view.h"
#include "ui/external_radar.h"
#include "ui/observatory_chrome.h"
#include <array>

namespace ui {
void drawExternalContactsStrip(lgfx::LGFX_Sprite &d, int top, const model::ContactsModel &m,
                               bool probes, size_t cursor, uint32_t now)
{
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    const auto key = model::contactKey(m, probes, cursor);
    const bool have = probes ? m.hasProbeSnapshot() : m.hasClientSnapshot();
    const uint32_t age = now - (probes ? m.probeSnapshotMs() : m.clientSnapshotMs());
    const bool fresh = have && age < 5000;
    const char *badge = !m.sniffing() ? "CACHED" : !have ? "WAIT" : fresh ? "LIVE" : "AGED";
    const BadgeTone tone = !m.sniffing() ? BadgeTone::Cached
                         : (have && fresh) ? BadgeTone::Live : BadgeTone::Warn;
    const bool cached = !m.sniffing();

    obsFrame(c, ModeSigil::NodeMesh, probes ? "REQUEST CLUSTER" : "AP / CLIENT CLUSTER", badge, tone);
    obsRadar(c, 84, 88);
    obsCard(c, 164, 36, 140, 103, key.valid ? kCalibrationYellow : kMutedSlate);
    obsDock(c, tone == BadgeTone::Live ? kFieldGreen : kMutedSlate, true);

    if (!key.valid) {
        obsMicro(c, 172, 42, probes ? "REQUESTED SSID" : "AP / BSSID", kMutedSlate, kPanelDark);
        obsText(c, 172, 60, probes ? "NO REQUESTS" : "NO LINKS", FontId::Title, kCalibrationYellow, 124);
        obsMicro(c, 172, 92, m.sniffing() ? "PASSIVE SNIFFER..." : "SNIFFER IDLE", kMutedSlate, kPanelDark);
        obsText(c, 12, 164, "PASSIVE SNIFFER", FontId::Title, kPaperPhosphor, 296);
        obsMicro(c, 12, 185, probes ? "NO SSID REQUESTS" : "NO CLIENT LINKS", kMutedSlate, kPanelDark);
        obsMicro(c, 308, 185, "RSSI @ PROBE / NO BEARING", kMutedSlate, kPanelDark, Align::Right);
        obsBay(c, 12, 94, "RSSI / dBm", "--", kCalibrationYellow);
        obsBay(c, 122, 86, "PACKETS", "--", kPaperPhosphor);
        obsBay(c, 224, 84, "AGE / s", "--", kMutedSlate);
        return;
    }

    const auto graph = model::contactGraph(m, key, false);
    std::array<RadarPoint, 12> nodes{};
    for (size_t i = 0; i < graph.size; ++i) {
        const size_t idx = graph.indices[i];
        const auto &mac = probes ? m.probes()[idx].mac : m.clients()[idx].mac;
        const int rssi = probes ? m.probes()[idx].rssi : m.clients()[idx].rssi;
        nodes[i] = radarPoint(mac, rssi, 84, 88);
        if (idx != cursor) c.line(84, 88, nodes[i].x, nodes[i].y, cached ? kLineBorder : kDimGreen);
    }
    if (graph.size) c.line(84, 88, nodes[0].x, nodes[0].y, kSignalPink);
    c.rect(78, 82, 13, 13, kVoidInk);
    obsDiamond(c, 84, 88, 4, kPaperPhosphor);
    c.dot(84, 88, 2, probes ? kCalibrationYellow : kFieldGreen);

    for (size_t i = graph.size; i > 0; --i) {
        obsRadarNode(c, nodes[i - 1], graph.indices[i - 1] == cursor, false, cached);
    }

    /* Right-side cluster dossier card. */
    obsMicro(c, 172, 42, probes ? "REQUESTED SSID" : "AP / BSSID", kMutedSlate, kPanelDark);
    obsText(c, 172, 55, key.target.empty() ? "<WILDCARD>" : key.target,
            FontId::Mono, kCalibrationYellow, 124);
    c.line(172, 72, 296, 72, kLineBorder);
    obsText(c, 172, 79, std::to_string(graph.total) + (probes ? " REQUESTERS" : " LINKS"),
            FontId::Title, kPaperPhosphor, 124);
    if (!probes) {
        const auto &row = m.clients()[cursor];
        obsBadge(c, 172, 99, 82, 14, "CH " + std::to_string(row.ch) + (row.band5 ? " 5G" : " 2.4G"),
                 kTrackDark, kDimGreen, kFieldGreen);
    } else {
        obsBadge(c, 172, 99, 98, 14, "NOT ASSOCIATION", kPanelDark, kLineBorder, kMutedSlate);
    }
    obsMicro(c, 172, 122, std::to_string(graph.size) + "/" + std::to_string(graph.total) + " DRAWN  @PROBE",
             kMutedSlate, kPanelDark);

    /* Lower Reading Dock. */
    obsText(c, 12, 164, key.mac, FontId::Title, kPaperPhosphor, 296);
    obsMicro(c, 12, 185, probes ? "OBSERVED REQUESTS" : "OBSERVED CLIENT LINKS", kMutedSlate, kPanelDark);
    obsMicro(c, 308, 185, "RSSI @ PROBE / NO BEARING", kMutedSlate, kPanelDark, Align::Right);

    const int rssi = probes ? m.probes()[cursor].rssi : m.clients()[cursor].rssi;
    const uint32_t packets = probes ? m.probes()[cursor].pkts : m.clients()[cursor].pkts;
    obsBay(c, 12, 94, "RSSI / dBm", std::to_string(rssi), kCalibrationYellow);
    obsBay(c, 122, 86, "PACKETS", std::to_string(packets), kPaperPhosphor);
    obsBay(c, 224, 84, "AGE / s", have ? std::to_string(age / 1000) : "--", kMutedSlate, have ? "s" : "");
}
}  // namespace ui
