/* gnss_fix_view.cpp — see gnss_fix_view.h. States and thresholds match
 * external_gnss_view.cpp so both screens always agree. */
#include "ui/gnss_fix_view.h"

#include <M5Cardputer.h>

#include <cmath>
#include <cstdio>

#include "ui/canvas.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {

void drawGnssFixView(const model::GnssModel &gps, uint32_t now, const ChromeState &chrome)
{
    auto &d = canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    const auto state = gps.state(now);
    const auto &fix = gps.fix();
    const uint32_t age_ms = gps.everFixed() ? gps.fixAgeMs(now) : 0;
    const bool fresh = state == model::GnssState::Fixed && age_ms < 3000;
    const bool uart = gps.hasUartData(now);

    d.setCursor(5, kBodyTop + 3);
    d.setTextColor(fresh ? kFieldGreen : uart ? kCalibrationYellow : kFaultRed, kVoidInk);
    d.print(fresh ? "LIVE FIX" : state == model::GnssState::Fixed ? "AGED FIX"
            : state == model::GnssState::FixLost ? "FIX LOST" : uart ? "SEARCHING" : "NO UART DATA");
    int hh = 0, mm = 0, ss = 0;
    d.setTextColor(kMutedSlate, kVoidInk);
    d.setCursor(178, kBodyTop + 3);
    if (model::splitUtcTime(fix.utc, &hh, &mm, &ss)) d.printf("%02d:%02d:%02dZ", hh, mm, ss);
    else d.print("--:--:--Z");
    d.drawFastHLine(5, kBodyTop + 14, 230, kDimGreen);

    char v[5][32];
    if (fresh) {
        std::snprintf(v[0], sizeof v[0], "%.5f %c", std::fabs(fix.lat_deg), fix.lat_deg >= 0 ? 'N' : 'S');
        std::snprintf(v[1], sizeof v[1], "%.5f %c", std::fabs(fix.lon_deg), fix.lon_deg >= 0 ? 'E' : 'W');
        std::snprintf(v[2], sizeof v[2], "%.1f m", fix.alt_m);
        const double h = fix.hdop;
        std::snprintf(v[3], sizeof v[3], "%.1f %s", h, h > 0.0 && h <= 1.5 ? "TIGHT" : h <= 3.0 ? "GOOD" : "WIDE");
    } else {
        for (int i = 0; i < 4; ++i) std::snprintf(v[i], sizeof v[i], "--");
    }
    if (gps.everFixed()) std::snprintf(v[4], sizeof v[4], "%lus", (unsigned long)(age_ms / 1000));
    else std::snprintf(v[4], sizeof v[4], "NEVER");
    static const char *const kLabels[] = {"LATITUDE", "LONGITUDE", "ALTITUDE", "HDOP", "FIX AGE"};
    for (int i = 0; i < 5; ++i) {
        const int y = kBodyTop + 20 + i * 12;
        d.setTextColor(kMutedSlate, kVoidInk);
        d.setCursor(8, y);
        d.print(kLabels[i]);
        d.setTextColor(i == 4 ? (fresh ? kFieldGreen : kCalibrationYellow) : fresh ? kPaperPhosphor : kMutedSlate,
                       kVoidInk);
        d.setCursor(96, y);
        d.print(v[i]);
    }

    d.drawFastHLine(5, kBodyTop + 86, 230, kDimGreen);
    d.setCursor(5, kHintRowY);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print("Enter OPEN WARDRIVE");
    endChrome(chrome);
}

}  // namespace ui
