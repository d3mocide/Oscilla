/*
 * wardrive_kml_test.cpp — host test for the storage::kml* functions.
 *
 * Normal invocation (no args) runs the PASS/FAIL unit checks below. Invoked
 * with --sample, it instead prints one fully-assembled document (header +
 * track + placemarks + footer) to stdout and does nothing else — that's
 * what tools/check_wardrive_kml.py feeds to Python's xml.etree to prove the
 * concatenated output is actually well-formed XML, not just individually
 * plausible-looking pieces (string-equality checks here can't catch a
 * mismatched tag between two functions the way a real parser can).
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <cstring>
#include <string>

#include "storage/wardrive_kml.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

model::ApRow ap(const std::string &ssid, const std::string &bssid, uint8_t ch, const std::string &auth)
{
    model::ApRow a;
    a.ssid = ssid;
    a.bssid = bssid;
    a.ch = ch;
    a.auth = auth;
    return a;
}

std::string sampleDocument()
{
    std::string doc = storage::kmlHeader("Test Session");
    doc += storage::kmlTrackPoint(47.283000, 8.565000, 450.0);
    doc += storage::kmlTrackPoint(47.283100, 8.565100, 451.0);
    doc += storage::kmlCloseTrack();
    doc += storage::kmlApPlacemark(ap("HomeNet", "aa:bb:cc:dd:ee:ff", 6, "WPA2"), 47.2831, 8.5651);
    doc += storage::kmlApPlacemark(ap("OpenCafe", "11:22:33:44:55:66", 11, "OPEN"), 47.2832, 8.5652);
    doc += storage::kmlApPlacemark(ap(std::string("\xC3\x28\xFF", 3), "22:33:44:55:66:77", 1, "OPEN"), 47.2833, 8.5653);
    doc += storage::kmlFooter();
    return doc;
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--sample") == 0) {
        std::fputs(sampleDocument().c_str(), stdout);
        return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "--recovery") == 0) {
        const std::string complete = sampleDocument();
        const size_t header_end = storage::kmlHeader("Test Session").size();
        for (size_t cut = header_end; cut < complete.size(); ++cut) {
            std::string repaired = storage::kmlRecoverPrefix(complete.substr(0, cut));
            std::fwrite(repaired.data(), 1, repaired.size(), stdout);
            std::fputc('\x1e', stdout);
        }
        return 0;
    }

    {
        std::string h = storage::kmlHeader("Session One");
        check(h.find("<?xml version=\"1.0\" encoding=\"UTF-8\"?>") == 0, "header starts with the XML prolog");
        check(h.find("xmlns=\"http://www.opengis.net/kml/2.2\"") != std::string::npos, "declares the KML 2.2 namespace");
        check(h.find("<name>Session One</name>") != std::string::npos, "session name embedded");
        check(h.find("<Style id=\"sOpen\">") != std::string::npos &&
              h.find("<Style id=\"sWep\">") != std::string::npos &&
              h.find("<Style id=\"sSecure\">") != std::string::npos &&
              h.find("<Style id=\"sEnterprise\">") != std::string::npos &&
              h.find("<Style id=\"sOther\">") != std::string::npos &&
              h.find("<Style id=\"sTrack\">") != std::string::npos,
              "all six style buckets present");
        check(h.find("<color>ff0000ff</color>") != std::string::npos, "open-AP color is aabbggrr red (ff0000ff), not rrggbb");
        check(h.find("<href>") == std::string::npos, "styles contain no remote icon resource");
        check(h.find("<coordinates>") != std::string::npos && h.find("</coordinates>") == std::string::npos,
              "header opens the track's <coordinates> but does not close it (incremental-append design)");
    }
    {
        std::string p = storage::kmlTrackPoint(47.283000, 8.565000, 450.0);
        check(p == "8.565000,47.283000,450.0\n", "track point is lon,lat,alt (KML order), not lat,lon");
    }
    {
        std::string p = storage::kmlTrackPoint(-33.868800, 151.209300, 10.0);
        check(p == "151.209300,-33.868800,10.0\n", "negative latitude (southern hemisphere) in a track point formats correctly");
    }
    check(storage::kmlCloseTrack() == "</coordinates>\n</LineString>\n</Placemark>\n",
          "closes coordinates/LineString/Placemark in the right order");

    {
        auto a = ap("HomeNet", "aa:bb:cc:dd:ee:ff", 6, "WPA2");
        std::string pm = storage::kmlApPlacemark(a, 47.2831, 8.5651);
        check(pm.find("<name>HomeNet</name>") != std::string::npos, "AP name uses its SSID");
        check(pm.find("<styleUrl>#sSecure</styleUrl>") != std::string::npos, "WPA2 buckets to the secure style");
        check(pm.find("<coordinates>8.565100,47.283100,0</coordinates>") != std::string::npos ||
              pm.find("<coordinates>8.565100,47.283100,0\n</coordinates>") != std::string::npos ||
              pm.find("8.565100,47.283100,0") != std::string::npos,
              "AP placemark coordinate is lon,lat,0 (KML order)");
    }
    {
        auto a = ap("", "00:11:22:33:44:55", 1, "OPEN");
        std::string pm = storage::kmlApPlacemark(a, 0.0, 0.0);
        check(pm.find("<name>00:11:22:33:44:55</name>") != std::string::npos,
              "hidden network (empty SSID) falls back to its BSSID as the placemark name");
        check(pm.find("<styleUrl>#sOpen</styleUrl>") != std::string::npos, "OPEN buckets to the open (red) style");
    }
    {
        auto a = ap("WEPNet", "00:00:00:00:00:01", 1, "WEP");
        check(storage::kmlApPlacemark(a, 0, 0).find("#sWep") != std::string::npos, "WEP buckets separately from WPA-family");
    }
    {
        auto a = ap("CorpNet", "00:00:00:00:00:02", 1, "WPA2-EAP");
        check(storage::kmlApPlacemark(a, 0, 0).find("#sEnterprise") != std::string::npos, "enterprise auth buckets separately");
    }
    {
        auto a = ap("Weird", "00:00:00:00:00:03", 1, "some-future-auth");
        check(storage::kmlApPlacemark(a, 0, 0).find("#sOther") != std::string::npos,
              "an unrecognized auth string buckets to sOther rather than crashing or guessing");
    }
    {
        // Hostile SSID: XML special chars + a raw control byte (BEL, 0x07)
        // that XML 1.0 forbids outright, even escaped.
        auto a = ap(std::string("<a & b> \"c'd\"\x07" "e"), "00:00:00:00:00:04", 1, "OPEN");
        std::string pm = storage::kmlApPlacemark(a, 0, 0);
        check(pm.find("<name>&lt;a &amp; b&gt; &quot;c&apos;d&quot;\\x07e</name>") != std::string::npos,
              "XML special chars entity-escaped and the forbidden control byte reversibly encoded");
        check(pm.find('\x07') == std::string::npos, "the raw control byte never reaches the output, escaped or not");
    }
    {
        auto a = ap(std::string("\xC3\x28\xFF", 3), "00:00:00:00:00:05", 1, "OPEN");
        std::string pm = storage::kmlApPlacemark(a, 0, 0);
        check(pm.find("\\xc3(\\xff") != std::string::npos,
              "malformed UTF-8 is emitted as reversible ASCII byte escapes");
        check(pm.find('\xC3') == std::string::npos && pm.find('\xFF') == std::string::npos,
              "malformed UTF-8 bytes never reach the XML output raw");
    }

    check(storage::kmlFooter() == "</Document>\n</kml>\n", "footer closes Document and kml");

    check(storage::kmlRecoverySuffix(storage::kmlFooter()).empty(), "complete KML needs no recovery suffix");
    check(storage::kmlRecoverySuffix(storage::kmlCloseTrack()) == storage::kmlFooter(),
          "closed track recovery appends only the document footer");
    check(storage::kmlRecoverySuffix("<coordinates>\n") == storage::kmlCloseTrack() + storage::kmlFooter(),
          "open track recovery closes the track and document");

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "wardrive kml test FAILED" : "wardrive kml test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
