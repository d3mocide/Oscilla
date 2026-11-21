/*
 * lora_log_format.cpp — see lora_log_format.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/lora_log_format.h"

#include <cstdio>

namespace storage {

namespace {

bool parseUtcHms(const std::string &utc, int *h, int *m, int *s)
{
    if (utc.size() < 6) return false;
    for (int i = 0; i < 6; ++i) {
        if (utc[i] < '0' || utc[i] > '9') return false;
    }
    int hh = (utc[0] - '0') * 10 + (utc[1] - '0');
    int mm = (utc[2] - '0') * 10 + (utc[3] - '0');
    int ss = (utc[4] - '0') * 10 + (utc[5] - '0');
    if (hh > 23 || mm > 59 || ss > 59) return false;
    *h = hh;
    *m = mm;
    *s = ss;
    return true;
}

std::string xmlEscape(const std::string &raw)
{
    std::string out;
    out.reserve(raw.size() + 8);
    static const char hex[] = "0123456789abcdef";
    for (unsigned char c : raw) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default:
            if ((c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) || c >= 0x80) {
                out += "\\x";
                out.push_back(hex[c >> 4]);
                out.push_back(hex[c & 0x0f]);
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    return out;
}

const char *snrStyleId(float snr)
{
    if (snr > 0.0f) return "sSnrGood";
    if (snr >= -10.0f) return "sSnrMid";
    return "sSnrWeak";
}

bool endsWith(const std::string &value, const std::string &suffix)
{
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

std::string loraLogHeader()
{
    return "ts_ms,freq_hz,sf,bw_khz,cr,rssi,snr,len,hex,framing,lat,lon,alt,hdop,fix_age_ms,utc\n";
}

std::string loraLogRow(uint32_t ts_ms, uint32_t freq_hz, int sf, int bw_khz, int cr,
                       const model::LoraPacket &p,
                       const model::GnssFix *fix,
                       uint32_t fix_age_ms)
{
    /* hex is at most LORA_MAX_PAYLOAD*2 = 510 chars; 720 leaves headroom for
     * numeric columns, framing name, and the six optional GNSS fields. */
    char buf[720];
    int n = 0;
    if (fix && fix->valid) {
        char utc_buf[64] = "";
        int h = 0, m = 0, s = 0;
        if (fix->year > 0 && parseUtcHms(fix->utc, &h, &m, &s)) {
            std::snprintf(utc_buf, sizeof utc_buf, "%04d-%02d-%02d %02d:%02d:%02d",
                          fix->year, fix->month, fix->day, h, m, s);
        }
        n = std::snprintf(buf, sizeof buf,
                          "%lu,%lu,%d,%d,%d,%d,%.1f,%u,%s,%s,%.6f,%.6f,%.1f,%.1f,%lu,%s\n",
                          (unsigned long)ts_ms, (unsigned long)freq_hz, sf, bw_khz, cr,
                          p.rssi, static_cast<double>(p.snr), (unsigned)p.len, p.hex.c_str(),
                          model::framingName(p.framing),
                          fix->lat_deg, fix->lon_deg, fix->alt_m,
                          static_cast<double>(fix->hdop), (unsigned long)fix_age_ms, utc_buf);
    } else {
        n = std::snprintf(buf, sizeof buf, "%lu,%lu,%d,%d,%d,%d,%.1f,%u,%s,%s,,,,,,\n",
                          (unsigned long)ts_ms, (unsigned long)freq_hz, sf, bw_khz, cr,
                          p.rssi, static_cast<double>(p.snr), (unsigned)p.len, p.hex.c_str(),
                          model::framingName(p.framing));
    }
    if (n < 0) return "";
    size_t len = static_cast<size_t>(n) < sizeof buf ? static_cast<size_t>(n) : sizeof buf - 1;
    return std::string(buf, len);
}

std::string loraKmlHeader(const std::string &session_name)
{
    std::string out;
    out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out += "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n";
    out += "<Document>\n";
    out += "<name>" + xmlEscape(session_name) + "</name>\n";
    /* KML color byte order is aabbggrr (green >0 dB, yellow -10..0, red <-10, pink waypoint). */
    out += "<Style id=\"sSnrGood\"><IconStyle><color>ff00c800</color></IconStyle></Style>\n";
    out += "<Style id=\"sSnrMid\"><IconStyle><color>ff00d6f5</color></IconStyle></Style>\n";
    out += "<Style id=\"sSnrWeak\"><IconStyle><color>ff0000ff</color></IconStyle></Style>\n";
    out += "<Style id=\"sWaypoint\"><IconStyle><color>ff9d5df0</color></IconStyle></Style>\n";
    return out;
}

std::string loraKmlPacketPlacemark(uint32_t ts_ms, uint32_t freq_hz, int sf, int bw_khz, int cr,
                                   const model::LoraPacket &p,
                                   double lat, double lon, double alt_m)
{
    char name_buf[64];
    std::snprintf(name_buf, sizeof name_buf, "%s %ddBm %+.1fdB",
                  model::framingName(p.framing), p.rssi, static_cast<double>(p.snr));
    char desc_buf[128];
    std::snprintf(desc_buf, sizeof desc_buf, "freq=%lu sf=%d bw=%d cr=%d len=%u ts=%lu",
                  (unsigned long)freq_hz, sf, bw_khz, cr, (unsigned)p.len, (unsigned long)ts_ms);
    char coord[96];
    std::snprintf(coord, sizeof coord, "%.6f,%.6f,%.1f", lon, lat, alt_m);

    std::string out;
    out += "<Placemark>\n<name>" + xmlEscape(name_buf) + "</name>\n";
    out += "<description>" + xmlEscape(desc_buf) + "</description>\n";
    out += "<styleUrl>#";
    out += snrStyleId(p.snr);
    out += "</styleUrl>\n<Point><coordinates>";
    out += coord;
    out += "</coordinates></Point>\n</Placemark>\n";
    return out;
}

std::string loraKmlWaypointPlacemark(uint32_t index, uint32_t ts_ms,
                                     double lat, double lon, double alt_m,
                                     const std::string &note)
{
    char name_buf[32];
    std::snprintf(name_buf, sizeof name_buf, "WP #%lu", (unsigned long)index);
    char desc_buf[96];
    std::snprintf(desc_buf, sizeof desc_buf, "ts=%lu %s", (unsigned long)ts_ms,
                  note.empty() ? "waypoint" : note.c_str());
    char coord[96];
    std::snprintf(coord, sizeof coord, "%.6f,%.6f,%.1f", lon, lat, alt_m);

    std::string out;
    out += "<Placemark>\n<name>" + xmlEscape(name_buf) + "</name>\n";
    out += "<description>" + xmlEscape(desc_buf) + "</description>\n";
    out += "<styleUrl>#sWaypoint</styleUrl>\n<Point><coordinates>";
    out += coord;
    out += "</coordinates></Point>\n</Placemark>\n";
    return out;
}

std::string loraKmlFooter()
{
    return "</Document>\n</kml>\n";
}

std::string loraKmlRecoverySuffix(const std::string &tail)
{
    const std::string footer = loraKmlFooter();
    return endsWith(tail, footer) ? "" : footer;
}

}  // namespace storage
