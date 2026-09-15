/*
 * gnss_view.cpp — see gnss_view.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/gnss_view.h"

#include <M5Cardputer.h>

#include "storage/wardrive_logger.h"
#include "ui/canvas.h"
#include "ui/link_view.h"

namespace ui {

namespace {

constexpr int kLineH = 10;

void label(M5Canvas &d, const char *text)
{
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.printf("  %-6s", text);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
}

/* One colour per state, so the antenna-unplug demo is readable across the
 * room rather than by reading the label. */
uint16_t stateColor(model::GnssState s)
{
    switch (s) {
        case model::GnssState::Fixed:      return TFT_GREEN;
        case model::GnssState::FixLost:    return TFT_ORANGE;
        case model::GnssState::Searching:  return TFT_YELLOW;
        case model::GnssState::NoUartData: return TFT_RED;
    }
    return TFT_DARKGREY;
}

void printAge(M5Canvas &d, uint32_t ms)
{
    if (ms < 1000) d.printf("%lums\n", (unsigned long)ms);
    else if (ms < 60000) d.printf("%lus\n", (unsigned long)(ms / 1000));
    else d.printf("%lum %lus\n", (unsigned long)(ms / 60000), (unsigned long)((ms / 1000) % 60));
}

}  // namespace

void drawGnssView(const model::GnssModel &gnss, uint32_t now_ms, const std::string &notice)
{
    auto &d = ui::canvas();
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);
    d.setCursor(0, 0);

    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.println("DRIVE");

    const model::GnssState st = gnss.state(now_ms);
    const model::GnssFix &fix = gnss.fix();

    d.setTextColor(TFT_MAGENTA, TFT_BLACK);
    d.print("GNSS    ");
    d.setTextColor(stateColor(st), TFT_BLACK);
    d.println(model::gnssStateName(st));

    /* "UART alive but no fix" is the whole point of the gate's antenna
     * test, so the link is stated on its own line rather than implied. */
    label(d, "uart");
    bool alive = gnss.hasUartData(now_ms);
    d.setTextColor(alive ? TFT_GREEN : TFT_RED, TFT_BLACK);
    d.println(alive ? "data" : "silent");

    if (gnss.everFixed()) {
        d.setTextColor(TFT_WHITE, TFT_BLACK);
        label(d, "lat");
        d.printf("%.6f\n", fix.lat_deg);
        label(d, "lon");
        d.printf("%.6f\n", fix.lon_deg);
        label(d, "alt");
        d.printf("%.1fm  hdop %.2f\n", fix.alt_m, (double)fix.hdop);
        label(d, "age");
        /* Age is only meaningful against a fix that exists; while one is
         * current it counts up from the last valid report either way. */
        d.setTextColor(st == model::GnssState::Fixed ? TFT_WHITE : TFT_ORANGE, TFT_BLACK);
        printAge(d, gnss.fixAgeMs(now_ms));
    } else {
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.println("  no fix yet this session");
    }

    if (fix.year > 0) {
        label(d, "utc");
        d.setTextColor(TFT_WHITE, TFT_BLACK);
        d.printf("%04d-%02d-%02d %s\n", fix.year, fix.month, fix.day,
                 fix.utc.empty() ? "" : fix.utc.substr(0, 6).c_str());
    }

    const auto &lg = storage::wardriveLogStats();
    d.println();
    d.setTextColor(TFT_MAGENTA, TFT_BLACK);
    d.print("LOG     ");
    if (!lg.open) {
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        d.println("idle");
    } else {
        d.setTextColor(TFT_GREEN, TFT_BLACK);
        d.println(storage::wardriveLogName());
        label(d, "rows");
        d.setTextColor(TFT_WHITE, TFT_BLACK);
        d.printf("%lu ap  %lu trk\n", (unsigned long)lg.aps, (unsigned long)lg.track_points);
        if (lg.aps_no_fix) {
            /* Dropped-for-no-fix is shown, never silently swallowed: it is
             * the other half of "both states logged distinctly". */
            label(d, "nofix");
            d.setTextColor(TFT_ORANGE, TFT_BLACK);
            d.printf("%lu dropped\n", (unsigned long)lg.aps_no_fix);
        }
    }

    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(0, d.height() - 2 * kLineH);
    d.print(printable(notice, 38).c_str());
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(0, d.height() - kLineH);
    d.print(", / cards   l log");
}

}  // namespace ui
