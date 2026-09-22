/*
 * legacy_log_format.cpp — see legacy_log_format.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/legacy_log_format.h"

#include <cstdio>

namespace storage {

std::string legacyLogHeader()
{
    return "ts_ms,freq_hz,rssi,len,hex\n";
}

std::string legacyLogRow(uint32_t ts_ms, uint32_t freq_hz, const model::LegacyPacket &p)
{
    /* hex is at most CC1101_MAX_CHUNK*2 = 128 chars; 200 leaves headroom
     * for the numeric columns and separators. */
    char buf[200];
    int n = std::snprintf(buf, sizeof buf, "%lu,%lu,%d,%u,%s\n",
                          (unsigned long)ts_ms, (unsigned long)freq_hz,
                          p.rssi, (unsigned)p.len, p.hex.c_str());
    if (n < 0) return "";
    size_t len = static_cast<size_t>(n) < sizeof buf ? static_cast<size_t>(n) : sizeof buf - 1;
    return std::string(buf, len);
}

}  // namespace storage
