/* North-up RAM breadcrumb with scale bar and quiet dock; Observatory v2. */
#include "ui/external_drive_view.h"
#include "ui/observatory_chrome.h"
#include <cmath>
#include <cstdio>

namespace ui {
namespace {
int niceScaleMeters(double meters) {
    static constexpr int kSteps[] = {5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000};
    for (int step : kSteps) {
        if (step >= meters) return step;
    }
    return 2000;
}
}  // namespace

void drawExternalDriveStrip(lgfx::LGFX_Sprite &d, int top, const model::GnssModel &gps,
                            const model::DriveTrail &trail, const storage::WardriveLogStats &log,
                            uint32_t now, bool closing)
{
    using namespace instrument;
    using namespace observatory;
    const bool fresh = gps.state(now) == model::GnssState::Fixed && gps.fixAgeMs(now) < 3000;
    const bool cached = trail.cached();
    const BadgeTone tone = cached ? BadgeTone::Cached : fresh ? BadgeTone::Live : BadgeTone::Warn;
    Canvas c(d, top);

    obsFrame(c, ModeSigil::VectorTrail, "DRIVE BREADCRUMB // N-UP",
             cached ? "CACHED" : fresh ? "FIX" : "NO FIX", tone);
    obsDock(c, fresh && !cached ? kFieldGreen : kCalibrationYellow, true);

    /* Subtle survey '+' crosshair grid inside the upper aperture. */
    c.box(20, 34, 280, 108, kTrackDark);
    for (int gx = 40; gx <= 280; gx += 40) {
        for (int gy = 46; gy <= 130; gy += 21) {
            c.line(gx - 2, gy, gx + 2, gy, kTrackDark);
            c.line(gx, gy - 2, gx, gy + 2, kTrackDark);
        }
    }

    /* Observatory north-up compass rose at top-right. */
    constexpr int rcx = 278, rcy = 59;
    c.dot(rcx, rcy, 11, kPanelDark);
    c.ring(rcx, rcy, 12, kDimGreen);
    c.line(rcx - 9, rcy, rcx - 6, rcy, kTrackDark);
    c.line(rcx + 6, rcy, rcx + 9, rcy, kTrackDark);
    c.line(rcx, rcy + 6, rcx, rcy + 9, kTrackDark);
    c.line(rcx, rcy - 3, rcx, rcy + 6, kFieldGreen);
    /* Crisp Signal Pink north arrowhead. */
    c.line(rcx, rcy - 8, rcx, rcy - 2, kSignalPink);
    c.line(rcx - 1, rcy - 6, rcx + 1, rcy - 6, kSignalPink);
    c.line(rcx - 2, rcy - 4, rcx + 2, rcy - 4, kSignalPink);
    c.line(rcx - 3, rcy - 2, rcx + 3, rcy - 2, kSignalPink);
    obsMicro(c, rcx, rcy - 21, "N", kCalibrationYellow, kVoidInk, Align::Center);

    if (!trail.size()) {
        obsTargetLock(c, 152, 84, 18, kCalibrationYellow);
        obsMicro(c, 152, 114, "WAITING FOR FRESH GNSS FIX", kCalibrationYellow, kVoidInk, Align::Center);
    } else {
        float east[model::DriveTrail::kCapacity], north[model::DriveTrail::kCapacity];
        double minx = 0, maxx = 0, miny = 0, maxy = 0;
        const auto &origin = trail.point(trail.size() - 1);
        const double cos_lat = std::cos(origin.lat * 0.0174532925199433);
        for (size_t i = 0; i < trail.size(); ++i) {
            const auto &p = trail.point(i);
            double lon = p.lon - origin.lon;
            if (lon > 180.0) lon -= 360.0;
            if (lon < -180.0) lon += 360.0;
            east[i] = float(lon * 111320.0 * cos_lat);
            north[i] = float((p.lat - origin.lat) * 111320.0);
            minx = std::min(minx, double(east[i])); maxx = std::max(maxx, double(east[i]));
            miny = std::min(miny, double(north[i])); maxy = std::max(maxy, double(north[i]));
        }
        const double scale = std::max(std::max((maxx - minx) / 215.0, (maxy - miny) / 62.0), 0.5);
        int lastx = 0, lasty = 0;
        const size_t half = trail.size() / 2;
        for (size_t i = 0; i < trail.size(); ++i) {
            const int x = 152 + int((east[i] - (minx + maxx) * 0.5) / scale);
            const int y = 79 - int((north[i] - (miny + maxy) * 0.5) / scale);
            if (i && !trail.point(i).gap) {
                const uint16_t seg_col = (cached || !fresh) ? kMutedSlate
                                       : (i < half ? kDimGreen : kFieldGreen);
                c.line(lastx, lasty, x, y, seg_col);
                if (i >= half && !cached && fresh) c.line(lastx, lasty + 1, x, y + 1, seg_col);
            }
            if (i == 0 && trail.size() > 1) {
                c.rect(x - 3, y - 3, 7, 7, kPanelDark);
                c.box(x - 3, y - 3, 7, 7, kFieldGreen);
                obsMicro(c, std::max(24, x - 9), y - 3, "S", kMutedSlate, kVoidInk);
            } else if (i + 1 == trail.size()) {
                obsDiamond(c, x, y, 8, kSignalPink);
                c.dot(x, y, 3, kCalibrationYellow);
            } else {
                c.rect(x - 1, y - 1, 2, 2, cached ? kMutedSlate : kPaperPhosphor);
            }
            lastx = x; lasty = y;
        }
        /* Dynamic scale bar directly from the computed meters/pixel scale. */
        const int bar_m = niceScaleMeters(scale * 44.0);
        const int bar_px = std::max(18, std::min(72, int(std::lround(double(bar_m) / scale))));
        c.line(28, 134, 28 + bar_px, 134, kMutedSlate);
        c.line(28, 131, 28, 137, kMutedSlate);
        c.line(28 + bar_px, 131, 28 + bar_px, 137, kMutedSlate);
        char sbuf[16];
        snprintf(sbuf, sizeof sbuf, "%dm", bar_m);
        obsMicro(c, 28 + bar_px / 2, 123, sbuf, kMutedSlate, kVoidInk, Align::Center);
    }
    obsMicro(c, 294, 131, "RAW GNSS / UNFILTERED", kMutedSlate, kVoidInk, Align::Right);

    /* Lower Reading Dock. */
    c.rect(12, 167, 6, 6, closing ? kCalibrationYellow : log.open ? kFieldGreen : kMutedSlate);
    obsText(c, 24, 164,
            closing ? "SESSION CLOSING" : log.open ? "SESSION RECORDING" : "SESSION IDLE",
            FontId::Title, log.open ? kFieldGreen : kPaperPhosphor, 198);

    char v[40];
    int hh = 0, mm = 0, ss = 0;
    if (model::splitUtcTime(gps.fix().utc, &hh, &mm, &ss)) {
        snprintf(v, sizeof v, "UTC %02d:%02d:%02dZ", hh, mm, ss);
        obsMicro(c, 308, 168, v, kMutedSlate, kPanelDark, Align::Right);
    }
    if (fresh) {
        snprintf(v, sizeof v, "%.5f %c  %.5f %c",
                 std::fabs(gps.fix().lat_deg), gps.fix().lat_deg >= 0 ? 'N' : 'S',
                 std::fabs(gps.fix().lon_deg), gps.fix().lon_deg >= 0 ? 'E' : 'W');
        obsMicro(c, 12, 185, v, kMutedSlate, kPanelDark);
        snprintf(v, sizeof v, "ALT %.0fm", gps.fix().alt_m);
        obsMicro(c, 308, 185, v, kMutedSlate, kPanelDark, Align::Right);
    } else {
        obsMicro(c, 12, 185, "FIX UNAVAILABLE", kCalibrationYellow, kPanelDark);
    }

    snprintf(v, sizeof v, "%.0fm", trail.distance());
    obsBay(c, 12, 94, "RAW PATH", trail.size() ? v : "--", kCalibrationYellow);
    obsBay(c, 122, 86, "SAVED APs", std::to_string(log.aps), kPaperPhosphor,
           log.open ? "REC" : "IDLE", log.open ? kFieldGreen : kMutedSlate);
    if (fresh) {
        snprintf(v, sizeof v, "%.1f", double(gps.fix().hdop));
        const char *tier = gps.fix().hdop < 1.5f ? "TIGHT" : gps.fix().hdop < 3.0f ? "GOOD" : "WIDE";
        obsBay(c, 224, 84, "HDOP", v, kFieldGreen, tier);
    } else {
        obsBay(c, 224, 84, "HDOP", "--", kCalibrationYellow, "WAIT");
    }
}
}  // namespace ui
