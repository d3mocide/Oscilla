/* RSSI constellation and selected access-point dossier; Observatory v2. */
#include "ui/external_wifi_view.h"
#include "ui/external_radar.h"
#include "ui/observatory_chrome.h"
#include <cstdio>

namespace ui {
void drawExternalWifiStrip(lgfx::LGFX_Sprite &d, int top, const model::ScanModel &scan,
                           size_t cursor, uint32_t now, bool new_observation,
                           const model::ContactsModel *contacts)
{
    using namespace instrument;
    using namespace observatory;
    const auto &rows = scan.rows();
    const bool live = scan.continuousActive();
    const bool scanning = scan.scanning();
    const bool cached = !live && !scanning;
    Canvas c(d, top);

    obsFrame(c, ModeSigil::HexOrbit, "AP CONSTELLATION",
             live ? "LIVE" : scanning ? "SCAN" : "CACHED",
             live ? BadgeTone::Live : scanning ? BadgeTone::Warn : BadgeTone::Cached);
    obsRadar(c, 84, 88);

    const size_t first = firstVisible(rows.size(), cursor, 24);
    const size_t count = std::min(size_t(24), rows.size() - first);
    for (size_t i = first; i < first + count; ++i) {
        if (i != cursor) obsRadarNode(c, radarPoint(rows[i].bssid, rows[i].rssi, 84, 88), false, true, cached);
    }

    obsCard(c, 164, 36, 140, 103, cursor < rows.size() ? kCalibrationYellow : kMutedSlate);
    obsDock(c, cached ? kMutedSlate : kFieldGreen, true);

    if (cursor >= rows.size()) {
        obsMicro(c, 172, 43, "SELECTED AP", kMutedSlate, kPanelDark);
        obsText(c, 172, 62, "NO APs YET", FontId::Title, kCalibrationYellow, 124);
        obsMicro(c, 172, 92, scanning ? "PASSIVE SURVEY..." : "WAITING FOR SCAN", kMutedSlate, kPanelDark);
        obsText(c, 12, 165, "PASSIVE WI-FI SURVEY", FontId::Title, kPaperPhosphor, 296);
        obsMicro(c, 12, 185, "NO ACCESS POINTS OBSERVED", kMutedSlate, kPanelDark);
        obsMicro(c, 308, 185, "STRENGTH / NO BEARING", kMutedSlate, kPanelDark, Align::Right);
        obsBay(c, 12, 94, "SIGNAL / dBm", "--", kCalibrationYellow);
        obsBay(c, 122, 86, "APs OBSERVED", "0", kPaperPhosphor);
        obsBay(c, 224, 84, "SECURITY", "--", kMutedSlate);
        return;
    }

    const auto &r = rows[cursor];
    const auto point = radarPoint(r.bssid, r.rssi, 84, 88);
    if (new_observation) c.ring(point.x, point.y, 10 + (now / 100) % 4, kDimGreen);

    size_t linked = 0;
    if (contacts && contacts->hasClientSnapshot()) {
        for (const auto &peer : contacts->clients()) {
            if (peer.bssid != r.bssid) continue;
            ++linked;
            if (linked > 12) continue;
            const auto node = radarPoint(peer.mac, peer.rssi, 84, 88);
            c.line(point.x, point.y, node.x, node.y, cached ? kLineBorder : kDimGreen);
            c.dot(node.x, node.y, 2, cached ? kMutedSlate : kFieldGreen);
        }
    }
    obsRadarNode(c, point, true, true, cached);
    /* Crisp leader connecting target lock to the dossier card. */
    const int lx = std::min(148, point.x + 11);
    c.line(lx, point.y, 148, point.y, kSignalPink);
    c.line(148, point.y, 164, 56, kSignalPink);

    /* Right-side Selected AP dossier card. */
    char ch_buf[16];
    snprintf(ch_buf, sizeof ch_buf, "CH %02u", unsigned(r.ch));
    obsMicro(c, 172, 42, "SELECTED AP", kMutedSlate, kPanelDark);
    obsText(c, 172, 55, ch_buf, FontId::Hero, kCalibrationYellow, 66);
    obsBadge(c, 240, 55, 56, 15, r.band5 ? "5 GHz" : "2.4 GHz", kTrackDark, kDimGreen, kPaperPhosphor);
    c.line(172, 78, 296, 78, kLineBorder);

    std::string sec = safe(r.auth, 10);
    if (r.mfp_required) sec += " MFP!";
    else if (r.mfp_capable) sec += " MFP";
    obsText(c, 172, 84, sec.empty() ? "OPEN" : sec, FontId::Mono, kFieldGreen, 124);
    obsMicro(c, 172, 99, linked ? std::to_string(linked) + " CLIENTS / CACHE" : "NO CLIENT CACHE",
             linked ? kPaperPhosphor : kMutedSlate, kPanelDark);
    obsMicro(c, 172, 113, "RSSI STRENGTH", kMutedSlate, kPanelDark);
    const int pct = std::max(0, std::min(100, (r.rssi + 100) * 100 / 70));
    obsSegBar(c, 172, 123, 124, 9, 14, pct, cached ? kMutedSlate : kFieldGreen);

    /* Lower Reading Dock. */
    obsText(c, 12, 164, r.ssid.empty() ? "<HIDDEN>" : r.ssid, FontId::Title, kPaperPhosphor, 296);
    obsText(c, 12, 184, r.bssid, FontId::Mono, kMutedSlate, 164);
    obsMicro(c, 308, 186, "STRENGTH / NO BEARING", kMutedSlate, kPanelDark, Align::Right);

    size_t count5 = 0;
    for (const auto &row : rows) if (row.band5) ++count5;
    char bands[20];
    snprintf(bands, sizeof bands, "%uG2 %uG5", unsigned(rows.size() - count5), unsigned(count5));
    char iv[16] = "";
    if (r.interval_ms) snprintf(iv, sizeof iv, "%ums", unsigned(r.interval_ms));

    obsBay(c, 12, 94, "SIGNAL / dBm", std::to_string(r.rssi), kCalibrationYellow);
    obsBay(c, 122, 86, "APs OBSERVED", std::to_string(rows.size()), kPaperPhosphor, bands);
    obsBay(c, 224, 84, "SECURITY", r.auth.empty() ? "OPEN" : safe(r.auth, 6), kFieldGreen, iv);
}
}  // namespace ui
