/*
 * legacy_log_format_test.cpp — host test for storage::legacyLogRow/Header.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "storage/legacy_log_format.h"

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
    check(storage::legacyLogHeader() == "ts_ms,freq_hz,rssi,len,hex\n", "header is the fixed column list");

    {
        model::LegacyChunk c;
        c.rssi = -63;
        c.len = 5;
        c.hex = "0102030405";
        std::string row = storage::legacyLogRow(12345, 433920000, c);
        check(row == "12345,433920000,-63,5,0102030405\n", "row matches expected column order");
    }
    {
        /* Zero-length payload - nothing about the format should special-case it. */
        model::LegacyChunk c;
        c.rssi = -110;
        c.len = 0;
        c.hex = "";
        std::string row = storage::legacyLogRow(0, 433920000, c);
        check(row == "0,433920000,-110,0,\n", "zero-length payload formats cleanly");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "legacy log format test FAILED" : "legacy log format test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
