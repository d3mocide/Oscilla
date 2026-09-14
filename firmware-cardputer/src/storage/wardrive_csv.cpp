/*
 * wardrive_csv.cpp — see wardrive_csv.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/wardrive_csv.h"

#include <cstdio>

namespace storage {

namespace {

/* Kismet's munge_for_csv (kis_wiglecsvlogfile.cc), transcribed exactly:
 * printable ASCII 32-126 except ',' and '"' pass through; everything else
 * becomes a 3-digit octal escape. Raw SSID bytes are attacker-controlled
 * (AGENTS.md "decoded bytes stay hostile") and this format has no field
 * quoting, so this is the only thing standing between a hostile SSID and a
 * corrupted CSV row. */
std::string csvMunge(const std::string &raw)
{
    std::string out;
    out.reserve(raw.size());
    for (unsigned char c : raw) {
        if (c >= 32 && c <= 126 && c != ',' && c != '"') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('\\');
            out.push_back(static_cast<char>(((c >> 6) & 0x03) + '0'));
            out.push_back(static_cast<char>(((c >> 3) & 0x07) + '0'));
            out.push_back(static_cast<char>((c & 0x07) + '0'));
        }
    }
    return out;
}

/* protocol/ocp.h's OCP_AUTH_* strings -> WigleWifi-style bracket tags.
 * Method-level only; see wardrive_csv.h for why. */
std::string authModeTag(const std::string &auth)
{
    if (auth == "OPEN") return "[ESS]";
    if (auth == "WEP") return "[WEP][ESS]";
    if (auth == "WPA") return "[WPA-PSK][ESS]";
    if (auth == "WPA2") return "[WPA2-PSK][ESS]";
    if (auth == "WPA/WPA2") return "[WPA-PSK][WPA2-PSK][ESS]";
    if (auth == "WPA3") return "[WPA3-SAE][ESS]";
    if (auth == "WPA2/WPA3") return "[WPA2-PSK][WPA3-SAE][ESS]";
    if (auth == "WPA-EAP") return "[WPA-EAP][ESS]";
    if (auth == "WPA2-EAP") return "[WPA2-EAP][ESS]";
    if (auth == "WPA3-EAP") return "[WPA3-EAP][ESS]";
    if (auth == "WPA2/WPA3-EAP") return "[WPA2-EAP][WPA3-EAP][ESS]";
    if (auth == "WPA3-EAP192") return "[WPA3-EAP-SUITE-B-192][ESS]";
    if (auth == "OWE") return "[OWE][ESS]";
    if (auth == "WAPI") return "[WAPI-PSK][ESS]";
    if (auth == "DPP") return "[DPP][ESS]";
    return "[UNKNOWN][ESS]";   /* OCP_AUTH_UNKNOWN, or anything not recognized above */
}

/* 2.4 GHz: 2407+5*ch, except ch14's documented exception. 5 GHz: 5000+5*ch
 * (holds for every channel number Oscilla's own hop list uses). */
int channelToFreqMhz(uint8_t ch, bool band5)
{
    if (band5) return 5000 + 5 * static_cast<int>(ch);
    if (ch == 14) return 2484;
    return 2407 + 5 * static_cast<int>(ch);
}

}  // namespace

std::string wardriveCsvHeader(const std::string &fw_version)
{
    char buf[256];
    int n = std::snprintf(buf, sizeof buf,
        "WigleWifi-1.6,appRelease=%s,model=Cardputer ADV,release=%s,"
        "device=cardputer,display=cardputer,board=esp32s3,brand=Oscilla,"
        "star=Sol,body=3,subBody=0\n",
        fw_version.c_str(), fw_version.c_str());
    if (n < 0) return "";
    size_t len = static_cast<size_t>(n) < sizeof buf ? static_cast<size_t>(n) : sizeof buf - 1;
    return std::string(buf, len);
}

std::string wardriveCsvColumnHeader()
{
    return "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,CurrentLatitude,"
           "CurrentLongitude,AltitudeMeters,AccuracyMeters,RCOIs,MfgrId,Type\n";
}

std::string wardriveCsvRow(const model::ApRow &ap,
                           int year, int month, int day, int hour, int min, int sec,
                           double lat, double lon, double alt_m, float accuracy_m)
{
    std::string ssid = csvMunge(ap.ssid);
    std::string auth = authModeTag(ap.auth);
    int freq = channelToFreqMhz(ap.ch, ap.band5);

    /* SSID is at most 32 raw bytes -> up to 128 munged; auth tag up to ~40;
     * everything else is small fixed-width numerics. 512 leaves headroom. */
    char buf[512];
    int n = std::snprintf(buf, sizeof buf,
        "%s,%s,%s,%04d-%02d-%02d %02d:%02d:%02d,%u,%d,%d,%.6f,%.6f,%f,%.1f,,,WIFI\n",
        ap.bssid.c_str(), ssid.c_str(), auth.c_str(),
        year, month, day, hour, min, sec,
        (unsigned)ap.ch, freq, ap.rssi,
        lat, lon, static_cast<double>(alt_m), static_cast<double>(accuracy_m));
    if (n < 0) return "";
    size_t len = static_cast<size_t>(n) < sizeof buf ? static_cast<size_t>(n) : sizeof buf - 1;
    return std::string(buf, len);
}

}  // namespace storage
