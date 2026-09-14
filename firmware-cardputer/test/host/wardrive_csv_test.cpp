/*
 * wardrive_csv_test.cpp — host test for storage::wardriveCsv{Header,
 * ColumnHeader,Row}. Expected strings computed independently (the munge
 * escape by hand-running Kismet's own algorithm in Python, not by trusting
 * this same C++ code) rather than eyeballed.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "storage/wardrive_csv.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

model::ApRow ap(const std::string &ssid, const std::string &bssid, uint8_t ch,
                const std::string &auth, int rssi, bool band5)
{
    model::ApRow a;
    a.ssid = ssid;
    a.bssid = bssid;
    a.ch = ch;
    a.auth = auth;
    a.rssi = rssi;
    a.band5 = band5;
    return a;
}

}  // namespace

int main()
{
    check(storage::wardriveCsvHeader("0.2.0") ==
          "WigleWifi-1.6,appRelease=0.2.0,model=Cardputer ADV,release=0.2.0,"
          "device=cardputer,display=cardputer,board=esp32s3,brand=Oscilla,"
          "star=Sol,body=3,subBody=0\n",
          "session header matches the WigleWifi-1.6 metadata line");

    check(storage::wardriveCsvColumnHeader() ==
          "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,"
          "CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type\n",
          "column header is the fixed 14-column WigleWifi-1.6 list");

    {
        auto a = ap("HomeNet", "aa:bb:cc:dd:ee:ff", 6, "WPA2", -55, false);
        std::string row = storage::wardriveCsvRow(a, 2026, 9, 14, 18, 30, 5,
                                                  47.283, 8.565, 450.0f, 5.0f);
        check(row == "aa:bb:cc:dd:ee:ff,HomeNet,[WPA2-PSK][ESS],2026-09-14 18:30:05,6,2437,-55,"
                     "47.283000,8.565000,450.000000,5.0,,,WIFI\n",
              "2.4GHz WPA2 row: channel->frequency (ch6->2437MHz), fields in exact WigleWifi order");
    }
    {
        auto a = ap("Guest", "11:22:33:44:55:66", 14, "OPEN", -70, false);
        std::string row = storage::wardriveCsvRow(a, 2026, 1, 1, 0, 0, 0, 0.0, 0.0, 0.0f, 0.0f);
        check(row.find(",14,2484,") != std::string::npos, "channel 14's documented exception: 2484MHz, not 2477");
        check(row.find("[ESS]") != std::string::npos && row.find("[WEP]") == std::string::npos,
              "OPEN auth carries no crypto tag, just [ESS] - matches an open AP, not miscategorized");
    }
    {
        auto a = ap("Office5G", "de:ad:be:ef:00:01", 149, "WPA3", -60, true);
        std::string row = storage::wardriveCsvRow(a, 2026, 6, 1, 12, 0, 0, 0.0, 0.0, 0.0f, 0.0f);
        check(row.find(",149,5745,") != std::string::npos, "5GHz ch149 -> 5745MHz (5000+5*149)");
        check(row.find("[WPA3-SAE][ESS]") != std::string::npos, "WPA3 maps to the SAE tag, not PSK (WPA3-Personal's actual AKM)");
    }
    {
        // Hostile SSID: comma, quote, and a raw control byte (BEL, 0x07).
        // Expected munge precomputed independently in Python: a\054b\042c\007d
        auto a = ap(std::string("a,b\"c\x07" "d"), "00:00:00:00:00:00", 1, "OPEN", -80, false);
        std::string row = storage::wardriveCsvRow(a, 2026, 1, 1, 0, 0, 0, 0.0, 0.0, 0.0f, 0.0f);
        check(row.find(",a\\054b\\042c\\007d,") != std::string::npos,
              "hostile SSID (comma/quote/control byte) munged to the exact escape sequence, not corrupting the row");
    }
    {
        // Hidden network: empty SSID must still produce a well-formed (empty) field.
        auto a = ap("", "00:11:22:33:44:55", 1, "WPA2", -65, false);
        std::string row = storage::wardriveCsvRow(a, 2026, 1, 1, 0, 0, 0, 0.0, 0.0, 0.0f, 0.0f);
        check(row.find("00:11:22:33:44:55,,[WPA2-PSK]") != std::string::npos,
              "empty SSID (hidden network) yields an empty field, not a shifted row");
    }
    {
        // Southern/western hemisphere: negative lat/lon must format correctly.
        auto a = ap("Sydney", "00:00:00:00:00:01", 40, "WPA/WPA2", -50, true);
        std::string row = storage::wardriveCsvRow(a, 2026, 1, 1, 0, 0, 0, -33.868800, 151.209300, 40.0f, 3.5f);
        check(row.find(",-33.868800,151.209300,") != std::string::npos,
              "negative latitude (southern hemisphere) formats correctly, not dropped or mangled");
        check(row.find("[WPA-PSK][WPA2-PSK][ESS]") != std::string::npos, "WPA/WPA2 transition mode carries both PSK tags");
    }
    {
        auto a = ap("Corp", "00:00:00:00:00:02", 1, "WPA2-EAP", -60, false);
        std::string row = storage::wardriveCsvRow(a, 2026, 1, 1, 0, 0, 0, 0.0, 0.0, 0.0f, 0.0f);
        check(row.find("[WPA2-EAP][ESS]") != std::string::npos, "enterprise (802.1X) auth mapped distinctly from PSK");
    }
    {
        auto a = ap("Weird", "00:00:00:00:00:03", 1, "some-future-auth-string", -60, false);
        std::string row = storage::wardriveCsvRow(a, 2026, 1, 1, 0, 0, 0, 0.0, 0.0, 0.0f, 0.0f);
        check(row.find("[UNKNOWN][ESS]") != std::string::npos,
              "an unrecognized auth string (e.g. OCP_AUTH_UNKNOWN, or a future value this code doesn't know yet) "
              "is tagged UNKNOWN rather than silently defaulting to something misleading like open");
    }
    {
        auto a = ap("X", "00:00:00:00:00:04", 1, "OPEN", -60, false);
        std::string row = storage::wardriveCsvRow(a, 2026, 1, 1, 0, 0, 0, 0.0, 0.0, 0.0f, 0.0f);
        check(row.substr(row.size() - 9) == ",,,WIFI\n" || row.find(",,,WIFI\n") != std::string::npos,
              "row ends with blank RCOIs, blank MfgrId, then Type=WIFI (14 columns total, unlike Kismet's own "
              "12-column Wi-Fi row bug)");
        size_t commas = 0;
        for (char c : row) if (c == ',') commas++;
        check(commas == 13, "exactly 13 commas -> 14 columns, matching the declared column header");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "wardrive csv test FAILED" : "wardrive csv test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
