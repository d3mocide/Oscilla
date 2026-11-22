/*
 * external_hunt_view.cpp — see external_hunt_view.h; Observatory v2.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/external_hunt_view.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ui/display_text.h"
#include "ui/observatory_chrome.h"

namespace ui {

void drawExternalHuntStrip(lgfx::LGFX_Sprite &d, int top,
                           const model::SignalHunt &h, uint32_t now_ms)
{
    using namespace instrument;
    using namespace observatory;
    Canvas c(d, top);
    const bool stale = h.isStale(now_ms);
    const bool live = h.streaming() && !stale;
    const char *prox = model::SignalHunt::proximityLabel(h.currentRssi(), stale);

    obsFrame(c, ModeSigil::FixReticle, "SIGNAL HUNT // LOCK",
             live ? prox : h.streaming() ? "WAIT" : "HOLD",
             live ? (h.currentRssi() >= -50 ? BadgeTone::Warn : BadgeTone::Live) : BadgeTone::Cached);
    obsDock(c, live ? kSignalPink : kMutedSlate, true);

    /* Sparkline plot: -35 .. -105 dBm (x=42..300, y=34..102). */
    constexpr int x0 = 42, w = 258, top_y = 34, base_y = 102, gh = base_y - top_y;
    auto dbmToY = [](int dbm) {
        const int clamped = std::max(-105, std::min(-35, dbm));
        const int lift = std::max(2, std::min(gh - 2, (clamped + 105) * (gh - 2) / 70));
        return base_y - lift;
    };

    obsDashedH(c, x0, x0 + w, dbmToY(-40), kTrackDark, 2, 3);
    obsDashedH(c, x0, x0 + w, dbmToY(-70), kTrackDark, 2, 3);
    c.line(x0, base_y, x0 + w, base_y, kDimGreen);
    obsMicro(c, 38, dbmToY(-40) - 3, "-40", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, 38, dbmToY(-70) - 3, "-70", kMutedSlate, kVoidInk, Align::Right);
    obsMicro(c, 38, base_y - 4, "-105", kMutedSlate, kVoidInk, Align::Right);

    const size_t n = h.historyCount();
    if (n > 0) {
        /* Peak reference dashed line. */
        const int peak_y = dbmToY(h.peakRssi());
        obsDashedH(c, x0, x0 + w, peak_y, kSignalPink, 3, 3);

        int last_x = x0, last_y = dbmToY(h.historyAt(0));
        for (size_t i = 0; i < n; ++i) {
            const int x = x0 + static_cast<int>(i * w / (model::SignalHunt::kHistoryCap - 1));
            const int y = dbmToY(h.historyAt(i));
            if (i > 0) {
                const uint16_t seg_col = stale ? kMutedSlate
                                       : h.historyAt(i) >= -50 ? kSignalPink : kCalibrationYellow;
                c.line(last_x, last_y, x, y, seg_col);
                c.line(last_x, last_y + 1, x, y + 1, seg_col);
            }
            last_x = x;
            last_y = y;
        }
        obsDiamondFill(c, last_x, last_y, 3, stale ? kMutedSlate : kSignalPink);

        char badge[24];
        std::snprintf(badge, sizeof badge, "%d dBm %s", h.currentRssi(), prox);
        const int bw = int(std::strlen(badge)) * 6 + 10;
        obsBadge(c, x0 + w - bw, 32, bw, 15, badge, kPanelDark,
                 stale ? kMutedSlate : kSignalPink,
                 stale ? kMutedSlate : kCalibrationYellow);
    } else {
        obsMicro(c, x0 + w / 2, 66, "LISTENING FOR TARGET FRAMES...",
                 kCalibrationYellow, kVoidInk, Align::Center);
    }

    /* High-contrast 24-segment proximity meter (y=114..144). */
    obsMicro(c, 20, 114, "PROXIMITY METER", kMutedSlate, kVoidInk);
    obsMicro(c, 300, 114, h.audioEnabled() ? "GEIGER: ON" : "GEIGER: MUTED",
             h.audioEnabled() ? kCalibrationYellow : kMutedSlate, kVoidInk, Align::Right);
    const int pct = stale ? 0 : std::max(0, std::min(100, (h.currentRssi() + 100) * 100 / 65));
    const uint16_t bar_col = stale ? kMutedSlate
                           : h.currentRssi() >= -50 ? kSignalPink
                           : h.currentRssi() >= -68 ? kCalibrationYellow : kFieldGreen;
    obsSegBar(c, 20, 126, 280, 14, 24, pct, bar_col);

    /* Lower Reading Dock. */
    const std::string title = h.label().empty() ? h.mac() : printable(h.label(), 20);
    obsText(c, 12, 164, title, FontId::Title, kPaperPhosphor, 186);

    char mode_tag[20];
    if (h.kind() == model::HuntKind::WifiAp) std::snprintf(mode_tag, sizeof mode_tag, "WIFI CH%u", unsigned(h.channel()));
    else std::snprintf(mode_tag, sizeof mode_tag, "BT LE");
    obsBadge(c, 204, 164, 62, 15, mode_tag, kTrackDark, kDimGreen, kFieldGreen);
    obsMicro(c, 308, 168, "LOCK", kSignalPink, kPanelDark, Align::Right);
    obsMicro(c, 12, 185, h.mac().empty() ? "--" : h.mac(), kMutedSlate, kPanelDark);

    obsBay(c, 12, 94, "LIVE / dBm", stale ? "--" : std::to_string(h.currentRssi()),
           stale ? kMutedSlate : kCalibrationYellow);
    obsBay(c, 122, 86, "PEAK / dBm", h.samples() ? std::to_string(h.peakRssi()) : "--", kSignalPink);
    obsBay(c, 224, 84, "HITS", std::to_string(h.samples()), kFieldGreen,
           h.audioEnabled() ? "AUD" : "MUTE");
}

}  // namespace ui
