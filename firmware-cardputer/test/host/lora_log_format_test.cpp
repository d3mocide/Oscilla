/*
 * lora_log_format_test.cpp — host test for storage::loraLogRow/Header.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "storage/lora_log_format.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

}  // namespace

int main()
{
    check(storage::loraLogHeader() == "ts_ms,freq_hz,sf,bw_khz,cr,rssi,snr,len,hex,framing\n", "header is the fixed column list");

    {
        model::LoraPacket p;
        p.rssi = -67;
        p.snr = 12.0f;
        p.len = 10;
        p.hex = "0102030405060708090a";
        p.framing = model::Framing::Unknown;
        std::string row = storage::loraLogRow(12345, 910525000, 7, 62, 1, p);
        check(row == "12345,910525000,7,62,1,-67,12.0,10,0102030405060708090a,unknown\n", "row matches expected column order");
    }
    {
        /* Negative SNR and a short payload - nothing about the format
         * should special-case either. */
        model::LoraPacket p;
        p.rssi = -110;
        p.snr = -3.5f;
        p.len = 0;
        p.hex = "";
        p.framing = model::Framing::Unknown;
        std::string row = storage::loraLogRow(0, 433000000, 12, 125, 4, p);
        check(row == "0,433000000,12,125,4,-110,-3.5,0,,unknown\n", "zero-length payload and negative SNR format cleanly");
    }
    {
        model::LoraPacket p;
        p.rssi = -70;
        p.snr = 8.5f;
        p.len = 2;
        p.hex = "aabb";
        p.framing = model::Framing::Meshtastic;
        std::string row = storage::loraLogRow(999, 910525000, 7, 62, 1, p);
        check(row == "999,910525000,7,62,1,-70,8.5,2,aabb,meshtastic\n", "framing guess appears as the last column");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "lora log format test FAILED" : "lora log format test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
