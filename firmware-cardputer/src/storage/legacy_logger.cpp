/*
 * legacy_logger.cpp — see legacy_logger.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/legacy_logger.h"

#include <cstdio>

#include <SD.h>

#include "storage/legacy_log_format.h"
#include "storage/sd_storage.h"

namespace storage {

namespace {
constexpr const char *kDir = "/oscilla/legacy";
constexpr int kMaxSessionIndex = 9999;

File g_file;
bool g_open = false;
}  // namespace

bool legacyLogBegin()
{
    if (!ready()) return false;
    if (!lock()) return false;

    /* mkdir on an existing directory just fails harmlessly; either way the
     * directory exists afterward, which is all this cares about. */
    SD.mkdir("/oscilla");
    SD.mkdir(kDir);

    char path[48];
    bool found = false;
    for (int i = 1; i <= kMaxSessionIndex; i++) {
        std::snprintf(path, sizeof path, "%s/log_%04d.csv", kDir, i);
        if (!SD.exists(path)) { found = true; break; }
    }
    if (!found) { unlock(); return false; }

    g_file = SD.open(path, FILE_WRITE);
    g_open = static_cast<bool>(g_file);
    if (g_open) {
        g_file.print(legacyLogHeader().c_str());
        g_file.flush();
    }
    unlock();
    return g_open;
}

void legacyLogChunk(uint32_t freq_hz, const model::LegacyChunk &c)
{
    if (!g_open) return;
    if (!lock()) return;   /* a contended lock just means this row is dropped, not blocked */

    std::string row = legacyLogRow(millis(), freq_hz, c);
    g_file.print(row.c_str());
    g_file.flush();

    unlock();
}

void legacyLogEnd()
{
    if (!g_open) return;
    if (lock()) {
        g_file.close();
        unlock();
    }
    g_open = false;
}

}  // namespace storage
