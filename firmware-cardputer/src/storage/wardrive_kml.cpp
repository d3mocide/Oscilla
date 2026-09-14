/*
 * wardrive_kml.cpp — see wardrive_kml.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/wardrive_kml.h"

#include <cstdio>

namespace storage {

namespace {

/* XML 1.0 text-content escaping. Raw SSID bytes are hostile (AGENTS.md
 * "decoded bytes stay hostile"): the five predefined entities handle
 * &<>"' , but XML 1.0 §2.2 forbids most C0 control bytes in a document
 * outright — not even "&#7;" is legal for BEL — so those are dropped to a
 * safe placeholder instead of "escaped" (there is no valid escape for
 * them). Tab/LF/CR are the only C0 bytes XML actually allows and pass
 * through untouched. */
std::string xmlEscapeText(const std::string &raw)
{
    std::string out;
    out.reserve(raw.size());
    for (unsigned char c : raw) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default:
            if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) {
                out.push_back('.');   /* forbidden in XML 1.0 text content, even escaped */
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    return out;
}

/* DESIGN §9.2 asks for "colored by security/type"; these five tiers are
 * this implementation's own bucketing of OCP_AUTH_* (protocol/ocp.h), not
 * something DESIGN itself specifies further. Colors are aabbggrr (KML's
 * own order, verified against Google's KML reference, not web rrggbb). */
const char *styleIdFor(const std::string &auth)
{
    if (auth == "OPEN") return "sOpen";
    if (auth == "WEP") return "sWep";
    if (auth == "WPA" || auth == "WPA2" || auth == "WPA/WPA2" ||
        auth == "WPA3" || auth == "WPA2/WPA3") return "sSecure";
    if (auth == "WPA-EAP" || auth == "WPA2-EAP" || auth == "WPA3-EAP" ||
        auth == "WPA2/WPA3-EAP" || auth == "WPA3-EAP192") return "sEnterprise";
    return "sOther";   /* OWE, WAPI, DPP, UNKNOWN, or anything future/unrecognized */
}

}  // namespace

std::string kmlHeader(const std::string &session_name)
{
    std::string name = xmlEscapeText(session_name);
    std::string out;
    out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out += "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n";
    out += "<Document>\n";
    out += "<name>" + name + "</name>\n";
    /* red: open (insecure) */
    out += "<Style id=\"sOpen\"><IconStyle><color>ff0000ff</color>"
           "<Icon><href>http://maps.google.com/mapfiles/kml/pushpin/wht-pushpin.png</href></Icon>"
           "</IconStyle></Style>\n";
    /* orange: WEP */
    out += "<Style id=\"sWep\"><IconStyle><color>ff00a5ff</color>"
           "<Icon><href>http://maps.google.com/mapfiles/kml/pushpin/wht-pushpin.png</href></Icon>"
           "</IconStyle></Style>\n";
    /* green: WPA-family personal */
    out += "<Style id=\"sSecure\"><IconStyle><color>ff00c800</color>"
           "<Icon><href>http://maps.google.com/mapfiles/kml/pushpin/wht-pushpin.png</href></Icon>"
           "</IconStyle></Style>\n";
    /* blue: enterprise/802.1X */
    out += "<Style id=\"sEnterprise\"><IconStyle><color>ffff7800</color>"
           "<Icon><href>http://maps.google.com/mapfiles/kml/pushpin/wht-pushpin.png</href></Icon>"
           "</IconStyle></Style>\n";
    /* grey: OWE/WAPI/DPP/unknown */
    out += "<Style id=\"sOther\"><IconStyle><color>ffa0a0a0</color>"
           "<Icon><href>http://maps.google.com/mapfiles/kml/pushpin/wht-pushpin.png</href></Icon>"
           "</IconStyle></Style>\n";
    /* yellow: the drive track line */
    out += "<Style id=\"sTrack\"><LineStyle><color>ff00ffff</color><width>3</width></LineStyle></Style>\n";
    out += "<Placemark>\n<name>Track</name>\n<styleUrl>#sTrack</styleUrl>\n";
    out += "<LineString>\n<tessellate>1</tessellate>\n<coordinates>\n";
    return out;
}

std::string kmlTrackPoint(double lat, double lon, double alt_m)
{
    char buf[128];
    /* KML coordinate order is lon,lat[,alt] - the opposite of how lat/lon
     * are normally spoken, and easy to get backwards. */
    int n = std::snprintf(buf, sizeof buf, "%.6f,%.6f,%.1f\n", lon, lat, alt_m);
    if (n < 0) return "";
    size_t len = static_cast<size_t>(n) < sizeof buf ? static_cast<size_t>(n) : sizeof buf - 1;
    return std::string(buf, len);
}

std::string kmlCloseTrack()
{
    return "</coordinates>\n</LineString>\n</Placemark>\n";
}

std::string kmlApPlacemark(const model::ApRow &ap, double lat, double lon)
{
    std::string label = ap.ssid.empty() ? ap.bssid : ap.ssid;
    std::string name = xmlEscapeText(label);
    std::string desc = xmlEscapeText(ap.bssid + " " + ap.auth + " ch" + std::to_string((int)ap.ch));

    char coord[64];
    int cn = std::snprintf(coord, sizeof coord, "%.6f,%.6f,0\n", lon, lat);
    std::string coords(coord, cn > 0 && static_cast<size_t>(cn) < sizeof coord
                                     ? static_cast<size_t>(cn) : sizeof coord - 1);

    std::string out;
    out += "<Placemark>\n<name>" + name + "</name>\n<description>" + desc + "</description>\n";
    out += "<styleUrl>#";
    out += styleIdFor(ap.auth);
    out += "</styleUrl>\n<Point><coordinates>";
    out += coords;
    out += "</coordinates></Point>\n</Placemark>\n";
    return out;
}

std::string kmlFooter()
{
    return "</Document>\n</kml>\n";
}

}  // namespace storage
