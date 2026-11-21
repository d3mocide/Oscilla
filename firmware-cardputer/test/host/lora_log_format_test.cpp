/*
 * lora_log_format_test.cpp — host test for storage::loraLog* CSV & KML (WP-10).
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <cstring>
#include <string>

#include "storage/lora_log_format.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

std::string sampleLoraKml()
{
    model::LoraPacket good{1000, -67, 8.5f, 4, "01020304", model::Framing::MeshCore};
    model::LoraPacket mid{2000, -95, -4.0f, 2, "aabb", model::Framing::Meshtastic};
    model::LoraPacket weak{3000, -118, -12.5f, 2, "ccdd", model::Framing::LoRaWAN};
    std::string doc = storage::loraKmlHeader("log_0001");
    doc += storage::loraKmlPacketPlacemark(1000, 910525000, 7, 62, 1, good, 47.2831, 8.5651, 450.5);
    doc += storage::loraKmlPacketPlacemark(2000, 910525000, 7, 62, 1, mid, 47.2832, 8.5652, 451.0);
    doc += storage::loraKmlPacketPlacemark(3000, 910525000, 7, 62, 1, weak, 47.2833, 8.5653, 452.0);
    doc += storage::loraKmlWaypointPlacemark(1, 3500, 47.2834, 8.5654, 453.0, "<ridge & tower>\x07");
    doc += storage::loraKmlFooter();
    return doc;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--sample-kml") == 0) {
        std::fputs(sampleLoraKml().c_str(), stdout);
        return 0;
    }

    check(storage::loraLogHeader() ==
              "ts_ms,freq_hz,sf,bw_khz,cr,rssi,snr,len,hex,framing,lat,lon,alt,hdop,fix_age_ms,utc\n",
          "header is the 16-column geotagged column list");

    {
        model::LoraPacket p;
        p.rssi = -67;
        p.snr = 12.0f;
        p.len = 10;
        p.hex = "0102030405060708090a";
        p.framing = model::Framing::Unknown;
        std::string row = storage::loraLogRow(12345, 910525000, 7, 62, 1, p);
        check(row == "12345,910525000,7,62,1,-67,12.0,10,0102030405060708090a,unknown,,,,,,\n",
              "row without fix leaves six trailing GNSS columns empty");
    }
    {
        model::LoraPacket p;
        p.rssi = -110;
        p.snr = -3.5f;
        p.len = 0;
        p.hex = "";
        p.framing = model::Framing::Unknown;
        model::GnssFix invalid_fix;
        invalid_fix.valid = false;
        invalid_fix.lat_deg = 45.0;
        std::string row = storage::loraLogRow(0, 433000000, 12, 125, 4, p, &invalid_fix, 999);
        check(row == "0,433000000,12,125,4,-110,-3.5,0,,unknown,,,,,,\n",
              "invalid fix never leaks coordinates into CSV");
    }
    {
        model::LoraPacket p;
        p.rssi = -70;
        p.snr = 8.5f;
        p.len = 2;
        p.hex = "aabb";
        p.framing = model::Framing::Meshtastic;
        model::GnssFix fix;
        fix.valid = true;
        fix.lat_deg = 47.283100;
        fix.lon_deg = -122.419400;
        fix.alt_m = 123.4;
        fix.hdop = 1.2f;
        fix.year = 2026;
        fix.month = 11;
        fix.day = 21;
        fix.utc = "143025.00";
        std::string row = storage::loraLogRow(999, 910525000, 7, 62, 1, p, &fix, 420);
        check(row == "999,910525000,7,62,1,-70,8.5,2,aabb,meshtastic,47.283100,-122.419400,123.4,1.2,420,2026-11-21 14:30:25\n",
              "valid fix appends lat,lon,alt,hdop,fix_age_ms,utc");
    }
    {
        model::LoraPacket p;
        p.rssi = -70;
        p.snr = 8.5f;
        p.len = 2;
        p.hex = "aabb";
        p.framing = model::Framing::MeshCore;
        model::GnssFix fix;
        fix.valid = true;
        fix.lat_deg = 1.0;
        fix.lon_deg = 2.0;
        fix.alt_m = 3.0;
        fix.hdop = 1.0f;
        fix.year = 0;  /* no RMC date yet: never fabricate a date */
        std::string row = storage::loraLogRow(500, 910525000, 7, 62, 1, p, &fix, 100);
        check(row == "500,910525000,7,62,1,-70,8.5,2,aabb,meshcore,1.000000,2.000000,3.0,1.0,100,\n",
              "valid fix without calendar date leaves utc blank");
    }
    {
        std::string h = storage::loraKmlHeader("log_0001");
        check(h.find("sSnrGood") != std::string::npos &&
              h.find("sSnrMid") != std::string::npos &&
              h.find("sSnrWeak") != std::string::npos &&
              h.find("sWaypoint") != std::string::npos &&
              h.find("<href>") == std::string::npos,
              "LoRa KML header declares all four local styles with no remote href");
        model::LoraPacket good{10, -65, 5.0f, 1, "aa", model::Framing::MeshCore};
        model::LoraPacket mid{20, -95, -5.0f, 1, "bb", model::Framing::Unknown};
        model::LoraPacket weak{30, -118, -10.5f, 1, "cc", model::Framing::LoRaWAN};
        auto pm_good = storage::loraKmlPacketPlacemark(10, 910525000, 7, 62, 1, good, 47.0, -122.0, 10.0);
        auto pm_mid = storage::loraKmlPacketPlacemark(20, 910525000, 7, 62, 1, mid, 47.0, -122.0, 10.0);
        auto pm_weak = storage::loraKmlPacketPlacemark(30, 910525000, 7, 62, 1, weak, 47.0, -122.0, 10.0);
        check(pm_good.find("#sSnrGood") != std::string::npos &&
              pm_good.find("-122.000000,47.000000,10.0") != std::string::npos,
              "positive SNR uses sSnrGood and KML lon,lat,alt order");
        check(pm_mid.find("#sSnrMid") != std::string::npos, "SNR in [-10,0] uses sSnrMid");
        check(pm_weak.find("#sSnrWeak") != std::string::npos, "SNR < -10 uses sSnrWeak");
        auto wp = storage::loraKmlWaypointPlacemark(2, 4000, 47.1, -122.2, 5.0, "<bad&\x07>");
        check(wp.find("<name>WP #2</name>") != std::string::npos &&
              wp.find("#sWaypoint") != std::string::npos &&
              wp.find("&lt;bad&amp;\\x07&gt;") != std::string::npos &&
              wp.find('\x07') == std::string::npos,
              "LoRa waypoint escapes XML/control bytes and uses sWaypoint");
        check(storage::loraKmlRecoverySuffix(storage::loraKmlFooter()).empty(),
              "complete LoRa KML needs no recovery suffix");
        check(storage::loraKmlRecoverySuffix("</Placemark>\n") == storage::loraKmlFooter(),
              "unclosed LoRa KML appends footer");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "lora log format test FAILED" : "lora log format test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
