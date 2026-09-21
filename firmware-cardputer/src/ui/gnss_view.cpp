/*
 * gnss_view.cpp — see gnss_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/gnss_view.h"

#include <cstdio>

#include <M5Cardputer.h>

#include "storage/wardrive_logger.h"
#include "ui/canvas.h"
#include "ui/link_view.h"
#include "ui/list_row.h"
#include "ui/theme.h"

namespace ui {

namespace {

void drawRow(M5Canvas &d, int y, const char *label, const char *value, uint16_t color)
{
    d.setCursor(4, y);
    d.setTextColor(kMutedSlate, kVoidInk);
    d.print(label);
    d.setCursor(94, y);
    d.setTextColor(color, kVoidInk);
    d.print(value);
}

/* One colour per state, so the antenna-unplug demo is readable across the
 * room rather than by reading the label. */
uint16_t stateColor(model::GnssState s)
{
    switch (s) {
        case model::GnssState::Fixed:      return kFieldGreen;
        case model::GnssState::FixLost:    return kSignalPink;
        case model::GnssState::Searching:  return kCalibrationYellow;
        case model::GnssState::NoUartData: return kFaultRed;
    }
    return kMutedSlate;
}

void formatAge(char *out, size_t size, uint32_t ms)
{
    if (ms < 1000) std::snprintf(out, size, "%lums", (unsigned long)ms);
    else if (ms < 60000) std::snprintf(out, size, "%lus", (unsigned long)(ms / 1000));
    else std::snprintf(out, size, "%lum %lus", (unsigned long)(ms / 60000),
                       (unsigned long)((ms / 1000) % 60));
}

}  // namespace

void drawGnssView(const model::GnssModel &gnss, uint32_t now_ms, const ChromeState &chrome)
{
    auto &d = ui::canvas();
    beginChrome(chrome);
    d.setTextSize(1);

    const model::GnssState st = gnss.state(now_ms);
    const model::GnssFix &fix = gnss.fix();
    const bool alive = gnss.hasUartData(now_ms);
    const auto &lg = storage::wardriveLogStats();

    d.setCursor(4, kBodyTop + 2);
    d.setTextColor(stateColor(st), kVoidInk);
    d.printf("GNSS %s", model::gnssStateName(st));
    d.setCursor(154, kBodyTop + 2);
    d.setTextColor(alive ? kFieldGreen : kFaultRed, kVoidInk);
    d.print(alive ? "UART DATA" : "UART SILENT");

    if (gnss.everFixed()) {
        char position[32];
        std::snprintf(position, sizeof position, "%.4f %.4f", fix.lat_deg, fix.lon_deg);
        char quality[32];
        std::snprintf(quality, sizeof quality, "%.0fm / %.1f HDOP", fix.alt_m, (double)fix.hdop);
        char age[16];
        formatAge(age, sizeof age, gnss.fixAgeMs(now_ms));
        drawRow(d, kBodyTop + 16, "POSITION", position, kPaperPhosphor);
        drawRow(d, kBodyTop + 29, "ALT / HDOP", quality, kPaperPhosphor);
        drawRow(d, kBodyTop + 42, "FIX AGE", age,
                st == model::GnssState::Fixed ? kFieldGreen : kSignalPink);
    } else {
        drawRow(d, kBodyTop + 16, "POSITION", "NO FIX THIS SESSION", kCalibrationYellow);
        drawRow(d, kBodyTop + 29, "FIX AGE", "--", kMutedSlate);
        drawRow(d, kBodyTop + 42, "UTC", fix.year > 0 ? fix.utc.c_str() : "--", kMutedSlate);
    }

    char session[32];
    if (!lg.open) {
        std::snprintf(session, sizeof session, "IDLE");
    } else {
        std::snprintf(session, sizeof session, "%lu AP / %lu TRK", (unsigned long)lg.aps,
                      (unsigned long)lg.track_points);
    }
    drawRow(d, kBodyTop + 55, "WARDRIVE", session, lg.open ? kFieldGreen : kMutedSlate);

    d.setCursor(4, kDetailRowY);
    d.setTextColor(lg.open ? kFieldGreen : kMutedSlate, kVoidInk);
    if (lg.open) {
        d.printf("LOG %lu AP · %lu TRK", (unsigned long)lg.aps,
                 (unsigned long)lg.track_points);
    } else if (gnss.everFixed()) {
        d.printf("FIX AGE %lu ms · LOG IDLE", (unsigned long)gnss.fixAgeMs(now_ms));
    } else {
        d.print("NO FIX · LOG IDLE");
    }

    endChrome(chrome);
}

}  // namespace ui
