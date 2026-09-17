/*
 * wardrive_logger.cpp — see wardrive_logger.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/wardrive_logger.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

#include <SD.h>

#include "storage/sd_storage.h"
#include "storage/wardrive_csv.h"
#include "storage/wardrive_kml.h"
#include "storage/wardrive_sessions.h"

namespace storage {

namespace {

constexpr const char *kDir = "/oscilla/wardrive";
constexpr std::size_t kMaxDirectoryEntries = 20000;

File g_csv, g_kml;
bool g_open = false;
bool g_track_open = false;
bool g_card_fault = false;
char g_name[16] = "";
char g_csv_path[64] = "";
char g_kml_path[64] = "";
WardriveLogStats g_stats;

std::array<bool, static_cast<std::size_t>(sessions::kLastIndex) + 1> g_used_sessions{};

void recoverKml(const char *path);

bool appendVerified(File &file, const char *path, const std::string &text)
{
    if (file.print(text.c_str()) != text.size()) return false;
    file.flush();
    /* Arduino File::flush() is void and can hide an fsync failure. A stat of
     * the still-open file is the available public API health check. */
    return SD.exists(path);
}

bool discoverSessions()
{
    g_used_sessions.fill(false);

    File dir = SD.open(kDir, FILE_READ);
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return false;
    }

    std::size_t inspected = 0;
    while (inspected < kMaxDirectoryEntries) {
        File entry = dir.openNextFile(FILE_READ);
        if (!entry) break;

        char name[32] = "";
        std::snprintf(name, sizeof name, "%s", entry.name());
        entry.close();

        int index = 0;
        bool is_kml = false;
        if (sessions::parse(name, &index, &is_kml)) {
            g_used_sessions[static_cast<std::size_t>(index)] = true;
            if (is_kml) {
                char path[64];
                std::snprintf(path, sizeof path, "%s/%s", kDir, name);
                recoverKml(path);
            }
        }
        inspected++;
    }

    /* Do not silently allocate a name when the bounded directory walk was
     * truncated. */
    File extra = dir.openNextFile(FILE_READ);
    const bool truncated = static_cast<bool>(extra);
    if (extra) extra.close();
    dir.close();
    return !truncated;
}

void recoverKml(const char *path)
{
    File source = SD.open(path, FILE_READ);
    if (!source) return;

    const size_t size = source.size();
    const size_t tail_size = kmlFooter().size() + kmlCloseTrack().size() + 32;
    const size_t start = size > tail_size ? size - tail_size : 0;
    if (!source.seek(start)) { source.close(); return; }

    std::string tail;
    tail.resize(size - start);
    size_t got = tail.empty() ? 0 : source.read(reinterpret_cast<uint8_t *>(&tail[0]), tail.size());
    tail.resize(got);
    source.close();

    std::string suffix = kmlRecoverySuffix(tail);
    if (suffix.empty()) return;
    File repair = SD.open(path, FILE_APPEND);
    if (!repair) return;
    (void)(repair.write(reinterpret_cast<const uint8_t *>(suffix.data()), suffix.size()) == suffix.size());
    repair.flush();
    repair.close();
}

/* Do not attempt a footer after a verified card failure; that write would
 * fail the same way. The next explicit logging request owns remount/recovery. */
void closeDead()
{
    if (g_csv) g_csv.close();
    if (g_kml) g_kml.close();
    g_open = false;
    g_track_open = false;
    g_card_fault = true;
    markFault();
    g_stats.open = false;
    g_name[0] = '\0';
    g_csv_path[0] = '\0';
    g_kml_path[0] = '\0';
}

}  // namespace

bool wardriveLogBegin(const model::GnssFix &fix)
{
    if (g_open) return true;
    if (g_card_fault) {
        if (!remount()) return false;
        g_card_fault = false;
    } else if (!ready()) return false;
    if (!lock()) return false;

    SD.mkdir("/oscilla");
    SD.mkdir(kDir);

    /* A reset can leave a valid KML prefix without its fixed closing suffix.
     * Repair existing KML files during one bounded directory walk, then
     * allocate the first unused session number. */
    if (!discoverSessions()) {
        g_card_fault = true;
        markFault();
        unlock();
        return false;
    }
    const int index = sessions::firstFree(g_used_sessions.data(), g_used_sessions.size());
    if (!index) { unlock(); return false; }

    std::snprintf(g_name, sizeof g_name, "drive_%04d", index);
    std::snprintf(g_csv_path, sizeof g_csv_path, "%s/%s.csv", kDir, g_name);
    std::snprintf(g_kml_path, sizeof g_kml_path, "%s/%s.kml", kDir, g_name);

    g_csv = SD.open(g_csv_path, FILE_WRITE);
    g_kml = SD.open(g_kml_path, FILE_WRITE);
    g_open = static_cast<bool>(g_csv) && static_cast<bool>(g_kml);

    if (g_open) {
        bool ok = appendVerified(g_csv, g_csv_path, wardriveCsvHeader(OSCILLA_DECK_VER));
        ok = ok && appendVerified(g_csv, g_csv_path, wardriveCsvColumnHeader());

        /* Date in the session name when the receiver has one — it commonly
         * does even with no position fix (RTC-backed), which is why
         * GnssModel parses it independent of fix validity. */
        std::string session = g_name;
        if (fix.year > 0) {
            char stamp[40];   /* sized for int-range %d, not just sane dates */
            std::snprintf(stamp, sizeof stamp, " %04d-%02d-%02d", fix.year, fix.month, fix.day);
            session += stamp;
        }
        ok = ok && appendVerified(g_kml, g_kml_path, kmlHeader(session));
        if (ok) g_track_open = true;
        else closeDead();
    } else {
        if (g_csv) g_csv.close();
        if (g_kml) g_kml.close();
        g_name[0] = '\0';
        g_csv_path[0] = '\0';
        g_kml_path[0] = '\0';
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
    bool ok = appendVerified(g_kml, g_kml_path, point);

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

    bool ok = appendVerified(g_csv, g_csv_path, csv_row);
    if (ok && g_track_open) {
        const std::string close = kmlCloseTrack();
        ok = appendVerified(g_kml, g_kml_path, close);
        g_track_open = false;
    }
    ok = ok && appendVerified(g_kml, g_kml_path, kml_row);
    const std::string reopen = kmlTrackStart();
    ok = ok && appendVerified(g_kml, g_kml_path, reopen);
    g_track_open = ok;

    if (!ok) { closeDead(); unlock(); return; }

    g_stats.aps++;
    unlock();
}

void wardriveLogEnd()
{
    if (!g_open) return;
    if (lock()) {
        bool ok = true;
        if (g_track_open) {
            ok = appendVerified(g_kml, g_kml_path, kmlCloseTrack());
            g_track_open = false;
        }
        ok = ok && appendVerified(g_kml, g_kml_path, kmlFooter());
        if (ok) {
            g_kml.close();
            g_csv.close();
        } else {
            closeDead();
        }
        unlock();
    }
    g_open = false;
    g_stats.open = false;
    g_name[0] = '\0';
    g_csv_path[0] = '\0';
    g_kml_path[0] = '\0';
}

const WardriveLogStats &wardriveLogStats() { return g_stats; }

const char *wardriveLogName() { return g_name; }

}  // namespace storage
