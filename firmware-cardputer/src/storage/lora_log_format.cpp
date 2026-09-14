/*
 * lora_log_format.cpp — see lora_log_format.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/lora_log_format.h"

#include <cstdio>

namespace storage {

std::string loraLogHeader()
{
    return "ts_ms,freq_hz,sf,bw_khz,cr,rssi,snr,len,hex,framing\n";
}

std::string loraLogRow(uint32_t ts_ms, uint32_t freq_hz, int sf, int bw_khz, int cr,
                       const model::LoraPacket &p)
{
    /* hex is at most LORA_MAX_PAYLOAD*2 = 510 chars; 600 leaves headroom for
     * the numeric columns, framing name and separators. */
    char buf[600];
    int n = std::snprintf(buf, sizeof buf, "%lu,%lu,%d,%d,%d,%d,%.1f,%u,%s,%s\n",
                          (unsigned long)ts_ms, (unsigned long)freq_hz, sf, bw_khz, cr,
                          p.rssi, static_cast<double>(p.snr), (unsigned)p.len, p.hex.c_str(),
                          model::framingName(p.framing));
    if (n < 0) return "";
    size_t len = static_cast<size_t>(n) < sizeof buf ? static_cast<size_t>(n) : sizeof buf - 1;
    return std::string(buf, len);
}

}  // namespace storage
