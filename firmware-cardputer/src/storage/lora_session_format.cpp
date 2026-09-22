/*
 * lora_session_format.cpp — see lora_session_format.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/lora_session_format.h"

#include <cstdio>

namespace storage {

std::string loraSessionManifest(uint32_t started_ms, const model::LoraModel &lora)
{
    char text[192];
    std::snprintf(text, sizeof text,
                  "schema=oscilla-lora-session-v1\n"
                  "started_ms=%lu\nprofile=%s\nfreq_hz=%lu\nsf=%d\nbw_khz=%d\ncr=%d\n",
                  (unsigned long)started_ms, lora.profile().c_str(),
                  (unsigned long)lora.freqHz(), lora.sf(), lora.bwKhz(), lora.cr());
    return text;
}

std::string loraHealthHeader()
{
    return "ts_ms,rx,crc_err,header_err,irq_drop,radio_drop,ocp_drop,packet_rows,packet_drops,health_drops\n";
}

std::string loraHealthRow(uint32_t ts_ms, const model::LoraHealth &health,
                          uint32_t packet_rows, uint32_t packet_drops,
                          uint32_t health_drops)
{
    char row[192];
    std::snprintf(row, sizeof row, "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
                  (unsigned long)ts_ms, (unsigned long)health.rx,
                  (unsigned long)health.crc_err, (unsigned long)health.header_err,
                  (unsigned long)health.irq_drop, (unsigned long)health.radio_drop,
                  (unsigned long)health.ocp_drop, (unsigned long)packet_rows,
                  (unsigned long)packet_drops, (unsigned long)health_drops);
    return row;
}

}  // namespace storage
