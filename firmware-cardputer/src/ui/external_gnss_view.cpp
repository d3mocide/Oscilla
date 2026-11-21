/* HDOP precision-tier astrolabe reticle (never invented satellites); Observatory v2. */
#include "ui/external_gnss_view.h"
#include "ui/observatory_chrome.h"
#include <cmath>
#include <cstdio>

namespace ui {
void drawExternalGnssStrip(lgfx::LGFX_Sprite &d, int top, const model::GnssModel &gps, uint32_t now)
{
    using namespace instrument;
    using namespace observatory;
    const auto state = gps.state(now);
    const uint32_t age_ms = gps.everFixed() ? gps.fixAgeMs(now) : 0;
    const bool fresh = state == model::GnssState::Fixed && age_ms < 3000;
    const char *badge = fresh ? "LIVE FIX"
                      : state == model::GnssState::Fixed ? "AGED FIX"
                      : state == model::GnssState::FixLost ? "FIX LOST"
                      : gps.hasUartData(now) ? "SEARCH" : "NO UART";
    const BadgeTone tone = fresh ? BadgeTone::Live
                         : state == model::GnssState::NoUartData ? BadgeTone::Fault
                         : BadgeTone::Warn;
    Canvas c(d, top);

    obsFrame(c, ModeSigil::FixReticle, "GNSS // FIX RETICLE", badge, tone);
    obsDock(c, fresh ? kFieldGreen : kCalibrationYellow, true);

    /* Left precision astrolabe: concentric rings encode real HDOP tiers (<1.5 / <3.0 / <6.0). */
    constexpr int cx = 84, cy = 83;
    const float hdop = gps.fix().hdop;
    const int active_tier = !fresh ? 0 : (hdop > 0.0f && hdop <= 1.5f) ? 1 : (hdop <= 3.0f) ? 2 : 3;

    obsHexagon(c, cx, cy, 50, kTrackDark);
    obsDashedCircle(c, cx, cy, 43, active_tier == 3 ? kCalibrationYellow : kTrackDark, 32);
    c.ring(cx, cy, 29, active_tier == 2 ? kFieldGreen : kDimGreen);
    if (active_tier == 1) c.dot(cx, cy, 14, kTrackDark);
    c.ring(cx, cy, 15, active_tier == 1 ? kFieldGreen : kLineBorder);

    c.line(cx - 50, cy, cx - 19, cy, kDimGreen);
    c.line(cx + 19, cy, cx + 50, cy, kDimGreen);
    c.line(cx, cy - 50, cx, cy - 19, kDimGreen);
    c.line(cx, cy + 19, cx, cy + 50, kDimGreen);

    obsTargetLock(c, cx, cy, 10, fresh ? kCalibrationYellow : kSignalPink);
    if (fresh) {
        c.dot(cx, cy, 4, kFieldGreen);
        c.dot(cx, cy, 1, kPaperPhosphor);
    } else {
        c.line(cx - 4, cy - 4, cx + 4, cy + 4, kSignalPink);
        c.line(cx + 4, cy - 4, cx - 4, cy + 4, kSignalPink);
    }

    obsMicro(c, cx + 16, cy - 17, "1.5", active_tier == 1 ? kFieldGreen : kMutedSlate, kVoidInk);
    obsMicro(c, cx + 27, cy - 28, "3.0", active_tier == 2 ? kFieldGreen : kMutedSlate, kVoidInk);
    obsMicro(c, cx + 38, cy - 39, "6.0", active_tier == 3 ? kCalibrationYellow : kDimGreen, kVoidInk);
    obsMicro(c, cx, 138, "HDOP PRECISION TIERS", kMutedSlate, kVoidInk, Align::Center);

    /* Right-side GNSS telemetry card. */
    obsCard(c, 164, 36, 140, 103, fresh ? kFieldGreen : kSignalPink);
    obsMicro(c, 172, 42, "FIX QUALITY", kMutedSlate, kPanelDark);
    const char *tier_txt = fresh ? (active_tier == 1 ? "TIGHT <1.5" : active_tier == 2 ? "GOOD <3.0" : "WIDE >3.0")
                         : state == model::GnssState::FixLost ? "SIGNAL LOST"
                         : gps.hasUartData(now) ? "ACQUIRING" : "NO UART";
    obsText(c, 172, 55, tier_txt, FontId::Title, fresh ? kFieldGreen : kSignalPink, 124);
    c.line(172, 76, 296, 76, kLineBorder);

    char v[40];
    int hh = 0, mm = 0, ss = 0;
    obsMicro(c, 172, 83, "UTC CLOCK", kMutedSlate, kPanelDark);
    if (model::splitUtcTime(gps.fix().utc, &hh, &mm, &ss)) {
        snprintf(v, sizeof v, "%02d:%02d:%02dZ", hh, mm, ss);
        obsMicro(c, 296, 83, v, kPaperPhosphor, kPanelDark, Align::Right);
    } else {
        obsMicro(c, 296, 83, "--:--:--", kMutedSlate, kPanelDark, Align::Right);
    }

    obsMicro(c, 172, 98, "NMEA LINK", kMutedSlate, kPanelDark);
    obsMicro(c, 296, 98, gps.hasUartData(now) ? "UART ACTIVE" : "SILENT",
             gps.hasUartData(now) ? kFieldGreen : kFaultRed, kPanelDark, Align::Right);

    obsMicro(c, 172, 113, "FIX FRESHNESS (3S)", kMutedSlate, kPanelDark);
    const int fresh_pct = fresh ? std::max(8, int((3000 - std::min<uint32_t>(3000, age_ms)) * 100 / 3000)) : 0;
    obsSegBar(c, 172, 123, 124, 9, 12, fresh_pct, kFieldGreen);

    /* Lower Reading Dock. */
    const char *headline = fresh ? "POSITION FIXED"
                         : state == model::GnssState::FixLost ? "POSITION FIX LOST"
                         : state == model::GnssState::Fixed ? "POSITION FIX AGED"
                         : gps.hasUartData(now) ? "SEARCHING FOR FIX" : "RECEIVER SILENT";
    obsText(c, 12, 164, headline, FontId::Title, fresh ? kFieldGreen : kCalibrationYellow, 224);
    if (gps.fix().year > 0) {
        snprintf(v, sizeof v, "%04d-%02d-%02d", gps.fix().year, gps.fix().month, gps.fix().day);
        obsMicro(c, 308, 168, v, kMutedSlate, kPanelDark, Align::Right);
    }
    if (fresh) {
        snprintf(v, sizeof v, "%.5f %c   %.5f %c",
                 std::fabs(gps.fix().lat_deg), gps.fix().lat_deg >= 0 ? 'N' : 'S',
                 std::fabs(gps.fix().lon_deg), gps.fix().lon_deg >= 0 ? 'E' : 'W');
        obsText(c, 12, 184, v, FontId::Mono, kPaperPhosphor, 296);
    } else {
        obsMicro(c, 12, 185, gps.hasUartData(now) ? "NMEA RECEIVING / NO USABLE POSITION" : "CHECK RECEIVER CONNECTION",
                 kCalibrationYellow, kPanelDark);
    }

    if (fresh) snprintf(v, sizeof v, "%.1f", double(hdop));
    else snprintf(v, sizeof v, "--");
    obsBay(c, 12, 94, "HDOP", v, kCalibrationYellow);

    if (fresh) snprintf(v, sizeof v, "%.1f", gps.fix().alt_m);
    else snprintf(v, sizeof v, "--");
    obsBay(c, 122, 86, "ALTITUDE", v, kPaperPhosphor, fresh ? "m" : "");

    if (gps.everFixed()) snprintf(v, sizeof v, "%lus", (unsigned long)(age_ms / 1000));
    else snprintf(v, sizeof v, "--");
    obsBay(c, 224, 84, "FIX AGE", v, fresh ? kFieldGreen : kSignalPink);
}
}  // namespace ui
