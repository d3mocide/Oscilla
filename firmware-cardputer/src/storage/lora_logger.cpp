/*
 * lora_logger.cpp — see lora_logger.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/lora_logger.h"

#include <bitset>
#include <cstdio>

#include <SD.h>

#include "model/session_review.h"

#include "storage/lora_log_format.h"
#include "storage/lora_session_format.h"
#include "storage/sd_storage.h"
#include "storage/sd_activity.h"
#include "storage/write_digest.h"

namespace storage {

namespace {
constexpr const char *kDir = "/oscilla/lora";
constexpr int kMaxSessionIndex = 9999;
/* Four files per session (.csv/.meta/.health.csv/.kml); leave room for strays. */
constexpr int kMaxDirectoryEntries = 5 * kMaxSessionIndex;

File g_packet_file, g_health_file, g_kml_file;
char g_packet_path[48] = "";
char g_health_path[56] = "";
char g_kml_path[48] = "";
LoraLogStats g_stats;
WriteDigest g_packet_digest, g_health_digest;

/* A short print() is the failure signal; no per-row FAT lookup under the
 * shared SD/TFT bus lock (Rev D §5.2). */
bool appendVerified(File &file, const std::string &text)
{
    const size_t accepted = file.print(text.c_str());
    sdActivity().record(text.size(), accepted, millis());
    if (accepted != text.size()) return false;
    auto *digest = &file == &g_packet_file ? &g_packet_digest :
                   &file == &g_health_file ? &g_health_digest : nullptr;
    if (digest) digest->append(reinterpret_cast<const uint8_t *>(text.data()), text.size());
    file.flush();
    return true;
}

/* One directory pass. Probing for existence per candidate index is a FAT
 * lookup each, O(sessions^2) overall, and stalled the deck loop for
 * 6 s on the bench card (WORKLOG 2026-11-21). The File-returning iterator is no
 * better: it stats and opens each entry by path, which is O(n^2) again.
 * Returns 0 when no index is free or the walk can't be trusted. */
int nextFreeSession()
{
    File dir = SD.open(kDir, FILE_READ);
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return 0;
    }
    static std::bitset<kMaxSessionIndex + 1> used;
    used.reset();
    int inspected = 0;
    bool truncated = false;
    for (;;) {
        /* readdir only; the File iterator would stat and open every entry. */
        String path = dir.getNextFileName();
        if (path.isEmpty()) break;
        if (++inspected > kMaxDirectoryEntries) { truncated = true; break; }
        const int slash = path.lastIndexOf('/');
        model::SessionEntry e;
        if (model::sessionName(path.c_str() + slash + 1, model::SessionKind::Lora, e)) used.set(e.index);
    }
    dir.close();
    if (truncated) return 0;
    for (int i = 1; i <= kMaxSessionIndex; i++)
        if (!used.test(i)) return i;
    return 0;
}
}  // namespace

bool loraLogBegin(const model::LoraModel &lora)
{
    if (!ready() || !lora.hasConfig()) return false;
    if (!lock()) return false;

    /* mkdir on an existing directory just fails harmlessly; either way the
     * directory exists afterward, which is all this cares about. */
    SD.mkdir("/oscilla");
    SD.mkdir(kDir);
    g_packet_digest = WriteDigest{};
    g_health_digest = WriteDigest{};

    const int index = nextFreeSession();
    if (!index) { unlock(); return false; }
    char manifest_path[48];
    char session_name[20];
    std::snprintf(session_name, sizeof session_name, "log_%04d", index);
    std::snprintf(g_packet_path, sizeof g_packet_path, "%s/log_%04d.csv", kDir, index);
    std::snprintf(manifest_path, sizeof manifest_path, "%s/log_%04d.meta", kDir, index);
    std::snprintf(g_health_path, sizeof g_health_path, "%s/log_%04d.health.csv", kDir, index);
    std::snprintf(g_kml_path, sizeof g_kml_path, "%s/log_%04d.kml", kDir, index);
    /* Four lookups for the one chosen index, never per candidate. A walk
     * that ended early must not let FILE_WRITE truncate an old session. */
    if (SD.exists(g_packet_path) || SD.exists(manifest_path) || SD.exists(g_health_path) || SD.exists(g_kml_path)) {
        g_packet_path[0] = '\0';
        g_health_path[0] = '\0';
        g_kml_path[0] = '\0';
        unlock();
        return false;
    }

    File manifest = SD.open(manifest_path, FILE_WRITE);
    g_packet_file = SD.open(g_packet_path, FILE_WRITE);
    g_health_file = SD.open(g_health_path, FILE_WRITE);
    g_kml_file = SD.open(g_kml_path, FILE_WRITE);
    const bool opened = manifest && g_packet_file && g_health_file && g_kml_file;
    bool written = opened && appendVerified(manifest, loraSessionManifest(millis(), lora)) &&
                   appendVerified(g_packet_file, loraLogHeader()) &&
                   appendVerified(g_health_file, loraHealthHeader()) &&
                   appendVerified(g_kml_file, loraKmlHeader(session_name));
    if (manifest) manifest.close();
    if (!written) {
        if (g_packet_file) g_packet_file.close();
        if (g_health_file) g_health_file.close();
        if (g_kml_file) g_kml_file.close();
        g_packet_path[0] = '\0';
        g_health_path[0] = '\0';
        g_kml_path[0] = '\0';
        unlock();
        return false;
    }
    g_stats = {};
    g_stats.open = true;
    unlock();
    return true;
}

void loraLogPacket(uint32_t freq_hz, int sf, int bw_khz, int cr, const model::LoraPacket &p,
                   const model::GnssFix *fix, uint32_t fix_age_ms)
{
    if (!g_stats.open) return;
    if (!lock()) { g_stats.packet_drops++; return; }

    const uint32_t ts = millis();
    const bool has_fix = fix && fix->valid;
    std::string row = loraLogRow(ts, freq_hz, sf, bw_khz, cr, p, has_fix ? fix : nullptr, fix_age_ms);
    if (appendVerified(g_packet_file, row)) {
        g_stats.packet_rows++;
        if (has_fix) {
            g_stats.geotagged_rows++;
            if (g_kml_file) {
                std::string pm = loraKmlPacketPlacemark(ts, freq_hz, sf, bw_khz, cr, p,
                                                        fix->lat_deg, fix->lon_deg, fix->alt_m);
                (void)appendVerified(g_kml_file, pm);
            }
        }
    } else {
        g_stats.packet_drops++;
    }

    unlock();
}

bool loraLogWaypoint(const model::GnssFix &fix, uint32_t *out_index)
{
    if (!g_stats.open || !fix.valid || !g_kml_file) return false;
    if (!lock()) return false;
    const uint32_t next_idx = g_stats.waypoints + 1;
    std::string pm = loraKmlWaypointPlacemark(next_idx, millis(), fix.lat_deg, fix.lon_deg, fix.alt_m, "manual");
    const bool ok = appendVerified(g_kml_file, pm);
    if (ok) {
        g_stats.waypoints = next_idx;
        if (out_index) *out_index = next_idx;
    }
    unlock();
    return ok;
}

void loraLogHealth(const model::LoraHealth &health)
{
    if (!g_stats.open || !health.valid) return;
    if (!lock()) { g_stats.health_drops++; return; }
    std::string row = loraHealthRow(millis(), health, g_stats.packet_rows,
                                    g_stats.packet_drops, g_stats.health_drops);
    if (appendVerified(g_health_file, row)) g_stats.health_rows++;
    else g_stats.health_drops++;
    unlock();
}

void loraLogEnd()
{
    if (!g_stats.open) return;
    if (lock()) {
        if (g_kml_file) {
            (void)appendVerified(g_kml_file, loraKmlFooter());
            g_kml_file.close();
        }
        g_packet_file.close();
        g_health_file.close();
        unlock();
    }
    g_stats.open = false;
    g_packet_path[0] = '\0';
    g_health_path[0] = '\0';
    g_kml_path[0] = '\0';
}

const LoraLogStats &loraLogStats() { return g_stats; }

LoraLogReadback verifyLoraLog()
{
    LoraLogReadback result;
    result.packet_bytes = g_packet_digest.bytes;
    result.health_bytes = g_health_digest.bytes;
    if (!g_stats.open || !ready()) return result;
    if (result.packet_bytes > 8192 || result.health_bytes > 8192) {
        result.state = LoraReadbackState::TooLarge;
        return result;
    }
    if (!lock(20)) return result;
    auto verify = [](const char *path, const WriteDigest &expected) {
        File input = SD.open(path, FILE_READ);
        if (!input || input.size() != expected.bytes) { input.close(); return false; }
        WriteDigest readback;
        uint8_t block[256];
        while (readback.bytes < expected.bytes) {
            const int count = input.read(block, sizeof(block));
            if (count <= 0 || static_cast<size_t>(count) > sizeof(block) ||
                static_cast<uint32_t>(count) > expected.bytes - readback.bytes) {
                input.close();
                return false;
            }
            const size_t n = static_cast<size_t>(count);
            readback.append(block, n);
        }
        input.close();
        return readback.matches(expected);
    };
    result.state = verify(g_packet_path, g_packet_digest) && verify(g_health_path, g_health_digest) ?
                   LoraReadbackState::Match : LoraReadbackState::Mismatch;
    unlock();
    return result;
}

}  // namespace storage
