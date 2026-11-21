/* BLE advertiser constellation with selected-device dossier; Observatory v2. */
#include "ui/external_bt_view.h"
#include "ui/external_radar.h"
#include "ui/observatory_chrome.h"
#include <cstdio>

namespace ui {
void drawExternalBtStrip(lgfx::LGFX_Sprite &d, int top, const model::BtModel &bt, size_t cursor)
{
    using namespace instrument;
    using namespace observatory;
    const auto &rows = bt.devices();
    const bool active = bt.scanning() || bt.continuousActive() || bt.airtagActive();
    const bool cached = !active;
    Canvas c(d, top);

    obsFrame(c, ModeSigil::NodeMesh, "BLE CONSTELLATION",
             active ? "LIVE" : "CACHED",
             active ? BadgeTone::Live : BadgeTone::Cached);
    obsRadar(c, 84, 88);

    const size_t first = firstVisible(rows.size(), cursor, 24);
    const size_t count = std::min(size_t(24), rows.size() - first);
    int strongest = -127;
    for (const auto &r : rows) strongest = std::max(strongest, r.rssi);

    for (size_t i = first; i < first + count; ++i) {
        if (i == cursor) continue;
        const auto pt = radarPoint(rows[i].mac, rows[i].rssi, 84, 88);
        if (rows[i].tracker) {
            obsDiamond(c, pt.x, pt.y, 3, cached ? kMutedSlate : kSignalPink);
        } else {
            obsRadarNode(c, pt, false, false, cached);
        }
    }

    obsCard(c, 164, 36, 140, 103, cursor < rows.size() && rows[cursor].tracker ? kSignalPink : kCalibrationYellow);
    obsDock(c, cached ? kMutedSlate : kFieldGreen, true);

    if (cursor >= rows.size()) {
        obsMicro(c, 172, 42, "COMPANY ID", kMutedSlate, kPanelDark);
        obsText(c, 172, 60, "NO DEVICES", FontId::Title, kCalibrationYellow, 124);
        obsMicro(c, 172, 92, active ? "PASSIVE BLE SCAN..." : "WAITING FOR SCAN", kMutedSlate, kPanelDark);
        obsText(c, 12, 164, "PASSIVE BLE DISCOVERY", FontId::Title, kPaperPhosphor, 296);
        obsMicro(c, 12, 185, "NO ADVERTISERS OBSERVED", kMutedSlate, kPanelDark);
        obsMicro(c, 308, 185, "STRENGTH / NO BEARING", kMutedSlate, kPanelDark, Align::Right);
        obsBay(c, 12, 94, "SIGNAL / dBm", "--", kCalibrationYellow);
        obsBay(c, 122, 86, "DEVICES", "0", kPaperPhosphor);
        obsBay(c, 224, 84, "CLASS", "--", kMutedSlate);
        return;
    }

    const auto &r = rows[cursor];
    const auto pt = radarPoint(r.mac, r.rssi, 84, 88);
    obsRadarNode(c, pt, true, r.tracker, cached);
    const int lx = std::min(148, pt.x + 11);
    c.line(lx, pt.y, 148, pt.y, kSignalPink);
    c.line(148, pt.y, 164, 56, kSignalPink);

    obsMicro(c, 172, 42, "COMPANY ID", kMutedSlate, kPanelDark);
    obsText(c, 172, 55, r.mfr.empty() ? "UNKNOWN" : obsUpper(safe(r.mfr, 10)),
            FontId::Title, kPaperPhosphor, 124);
    c.line(172, 76, 296, 76, kLineBorder);
    obsBadge(c, 172, 81, 86, 14, r.tracker ? "TRACKER FLAG" : "ADVERTISER",
             r.tracker ? kPanelDark : kTrackDark,
             r.tracker ? kSignalPink : kDimGreen,
             r.tracker ? kSignalPink : kFieldGreen);
    obsMicro(c, 172, 100, std::to_string(std::max(0, strongest - r.rssi)) + "dB BELOW MAX",
             kCalibrationYellow, kPanelDark);
    obsMicro(c, 172, 113, "RSSI STRENGTH", kMutedSlate, kPanelDark);
    const int pct = std::max(0, std::min(100, (r.rssi + 100) * 100 / 70));
    obsSegBar(c, 172, 123, 124, 9, 14, pct, cached ? kMutedSlate : (r.tracker ? kSignalPink : kFieldGreen));

    obsText(c, 12, 164, r.name.empty() ? "UNNAMED DEVICE" : r.name, FontId::Title, kPaperPhosphor, 296);
    obsText(c, 12, 184, r.mac, FontId::Mono, kMutedSlate, 164);
    obsMicro(c, 308, 186, "STRENGTH / NO BEARING", kMutedSlate, kPanelDark, Align::Right);

    size_t trackers = 0;
    for (const auto &row : rows) if (row.tracker) ++trackers;
    char tbuf[16] = "";
    if (trackers) snprintf(tbuf, sizeof tbuf, "%u TRK", unsigned(trackers));

    obsBay(c, 12, 94, "SIGNAL / dBm", std::to_string(r.rssi), kCalibrationYellow);
    obsBay(c, 122, 86, "DEVICES", std::to_string(rows.size()), kPaperPhosphor, tbuf, kSignalPink);
    obsBay(c, 224, 84, "CLASS", r.tracker ? "TRACK" : "ADV", r.tracker ? kSignalPink : kFieldGreen);
}
}  // namespace ui
