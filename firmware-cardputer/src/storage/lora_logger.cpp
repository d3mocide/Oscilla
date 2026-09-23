/*
 * lora_logger.cpp — see lora_logger.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/lora_logger.h"

#include <cstdio>

#include <SD.h>

#include "storage/lora_log_format.h"
#include "storage/lora_session_format.h"
#include "storage/sd_storage.h"

namespace storage {

namespace {
constexpr const char *kDir = "/oscilla/lora";
constexpr int kMaxSessionIndex = 9999;

File g_packet_file, g_health_file;
char g_packet_path[48] = "";
char g_health_path[56] = "";
LoraLogStats g_stats;

/* A short print() is the failure signal; no per-row FAT lookup under the
 * shared SD/TFT bus lock (Rev D §5.2). */
bool appendVerified(File &file, const std::string &text)
{
    if (file.print(text.c_str()) != text.size()) return false;
    file.flush();
    return true;
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

    char manifest_path[48];
    bool found = false;
    for (int i = 1; i <= kMaxSessionIndex; i++) {
        std::snprintf(g_packet_path, sizeof g_packet_path, "%s/log_%04d.csv", kDir, i);
        std::snprintf(manifest_path, sizeof manifest_path, "%s/log_%04d.meta", kDir, i);
        std::snprintf(g_health_path, sizeof g_health_path, "%s/log_%04d.health.csv", kDir, i);
        if (!SD.exists(g_packet_path) && !SD.exists(manifest_path) && !SD.exists(g_health_path)) {
            found = true;
            break;
        }
    }
    if (!found) { unlock(); return false; }

    File manifest = SD.open(manifest_path, FILE_WRITE);
    g_packet_file = SD.open(g_packet_path, FILE_WRITE);
    g_health_file = SD.open(g_health_path, FILE_WRITE);
    const bool opened = manifest && g_packet_file && g_health_file;
    bool written = opened && appendVerified(manifest, loraSessionManifest(millis(), lora)) &&
                   appendVerified(g_packet_file, loraLogHeader()) &&
                   appendVerified(g_health_file, loraHealthHeader());
    if (manifest) manifest.close();
    if (!written) {
        if (g_packet_file) g_packet_file.close();
        if (g_health_file) g_health_file.close();
        g_packet_path[0] = '\0';
        g_health_path[0] = '\0';
        unlock();
        return false;
    }
    g_stats = {};
    g_stats.open = true;
    unlock();
    return true;
}

void loraLogPacket(uint32_t freq_hz, int sf, int bw_khz, int cr, const model::LoraPacket &p)
{
    if (!g_stats.open) return;
    if (!lock()) { g_stats.packet_drops++; return; }

    std::string row = loraLogRow(millis(), freq_hz, sf, bw_khz, cr, p);
    if (appendVerified(g_packet_file, row)) g_stats.packet_rows++;
    else g_stats.packet_drops++;

    unlock();
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
        g_packet_file.close();
        g_health_file.close();
        unlock();
    }
    g_stats.open = false;
    g_packet_path[0] = '\0';
    g_health_path[0] = '\0';
}

const LoraLogStats &loraLogStats() { return g_stats; }

}  // namespace storage
