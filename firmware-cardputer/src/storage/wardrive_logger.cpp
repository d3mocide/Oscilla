/*
 * wardrive_logger.cpp — see wardrive_logger.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/wardrive_logger.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#include <SD.h>

#include "storage/sd_storage.h"
#include "storage/wardrive_csv.h"
#include "storage/wardrive_kml.h"

namespace storage {

namespace {

constexpr const char *kDir = "/oscilla/wardrive";
constexpr int kMaxSessionIndex = 9999;

File g_csv, g_kml;
bool g_open = false;
bool g_track_open = false;
char g_name[16] = "";
WardriveLogStats g_stats;

/* A write returning short (almost always 0) is the only signal Arduino's SD
 * layer gives for "the card is gone" - nothing else here polls for
 * presence. Confirmed live 2026-09-15: without this, g_open stays true
 * forever once set, so a session survives a card pull indefinitely and
 * keeps counting aps_no_fix in RAM against a card that no longer exists,
 * completely decoupled from reality (see WORKLOG). Don't attempt the KML
 * footer here - that write would fail the same way - just stop pretending. */
void closeDead()
{
    if (g_csv) g_csv.close();
    if (g_kml) g_kml.close();
    g_open = false;
    g_track_open = false;
    g_stats.open = false;
    g_name[0] = '\0';
}

}  // namespace

bool wardriveLogBegin(const model::GnssFix &fix)
{
    if (g_open) return true;
    if (!ready()) return false;
    if (!lock()) return false;

    SD.mkdir("/oscilla");
    SD.mkdir(kDir);

    char csv_path[64], kml_path[64];
    bool found = false;
    for (int i = 1; i <= kMaxSessionIndex; i++) {
        std::snprintf(g_name, sizeof g_name, "drive_%04d", i);
        std::snprintf(csv_path, sizeof csv_path, "%s/%s.csv", kDir, g_name);
        std::snprintf(kml_path, sizeof kml_path, "%s/%s.kml", kDir, g_name);
        if (!SD.exists(csv_path) && !SD.exists(kml_path)) { found = true; break; }
    }
    if (!found) { g_name[0] = '\0'; unlock(); return false; }

    g_csv = SD.open(csv_path, FILE_WRITE);
    g_kml = SD.open(kml_path, FILE_WRITE);
    g_open = static_cast<bool>(g_csv) && static_cast<bool>(g_kml);

    if (g_open) {
        g_csv.print(wardriveCsvHeader(OSCILLA_DECK_VER).c_str());
        g_csv.print(wardriveCsvColumnHeader().c_str());
        g_csv.flush();

        /* Date in the session name when the receiver has one — it commonly
         * does even with no position fix (RTC-backed), which is why
         * GnssModel parses it independent of fix validity. */
        std::string session = g_name;
        if (fix.year > 0) {
            char stamp[40];   /* sized for int-range %d, not just sane dates */
            std::snprintf(stamp, sizeof stamp, " %04d-%02d-%02d", fix.year, fix.month, fix.day);
            session += stamp;
        }
        g_kml.print(kmlHeader(session).c_str());
        g_kml.flush();
        g_track_open = true;
    } else {
        if (g_csv) g_csv.close();
        if (g_kml) g_kml.close();
        g_name[0] = '\0';
    }

    g_stats = WardriveLogStats{};
    g_stats.open = g_open;
    unlock();
    return g_open;
}

void wardriveLogTrackPoint(const model::GnssFix &fix)
{
    if (!g_open || !g_track_open || !fix.valid) return;
    if (!lock()) return;   /* a contended lock drops this vertex, never blocks */

    std::string point = kmlTrackPoint(fix.lat_deg, fix.lon_deg, fix.alt_m);
    bool ok = g_kml.print(point.c_str()) == point.size();
    g_kml.flush();

    if (!ok) { closeDead(); unlock(); return; }

    g_stats.track_points++;
    unlock();
}

void wardriveLogAp(const model::ApRow &ap, const model::GnssFix &fix, uint32_t fix_age_ms)
{
    if (!g_open) return;

    /* Counted, not written: a positionless WigleWifi row is wrong data, not
     * partial data (see the header). The KML track's gap says the same thing
     * geometrically. */
    if (!fix.valid) { g_stats.aps_no_fix++; return; }
    (void)fix_age_ms;   /* accuracy_m stays 0: no verified UERE constant (wardrive_csv.h) */

    if (!lock()) return;

    int h, m, s;
    model::splitUtcTime(fix.utc, &h, &m, &s);
    std::string csv_row = wardriveCsvRow(ap, fix.year, fix.month, fix.day, h, m, s,
                                          fix.lat_deg, fix.lon_deg, fix.alt_m, 0.0f);
    std::string kml_row = kmlApPlacemark(ap, fix.lat_deg, fix.lon_deg);

    bool ok = g_csv.print(csv_row.c_str()) == csv_row.size();
    g_csv.flush();
    ok = ok && (g_kml.print(kml_row.c_str()) == kml_row.size());
    g_kml.flush();

    if (!ok) { closeDead(); unlock(); return; }

    g_stats.aps++;
    unlock();
}

void wardriveLogEnd()
{
    if (!g_open) return;
    if (lock()) {
        if (g_track_open) {
            g_kml.print(kmlCloseTrack().c_str());
            g_track_open = false;
        }
        g_kml.print(kmlFooter().c_str());
        g_kml.close();
        g_csv.close();
        unlock();
    }
    g_open = false;
    g_stats.open = false;
    g_name[0] = '\0';
}

const WardriveLogStats &wardriveLogStats() { return g_stats; }

const char *wardriveLogName() { return g_name; }

}  // namespace storage
