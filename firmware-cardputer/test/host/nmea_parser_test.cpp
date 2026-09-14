/*
 * nmea_parser_test.cpp — host test for gnss::NmeaParser. Adversarial first,
 * per AGENTS.md §6: garbage, torn checksums and chunk boundaries before
 * anything that merely looks like a fix.
 *
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "gnss/nmea_parser.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

/* Known-good fixtures: checksums computed independently (XOR of bytes
 * between '$' and '*'), not by trusting this same parser. */
const char *kGgaFix   = "$GNGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*45\r\n";
const char *kGgaNoFix = "$GNGGA,092725.00,,,,,0,00,99.99,,M,,M,,*73\r\n";
const char *kRmcFix   = "$GNRMC,092725.00,A,4717.11399,N,00833.91590,E,0.004,77.52,091202,,,A*4A\r\n";
const char *kRmcNoFix = "$GNRMC,092725.00,V,,,,,,,091202,,,N*60\r\n";
const char *kGsv      = "$GPGSV,3,1,11,10,63,137,17,07,61,098,15,05,59,290,20,08,54,157,30*70\r\n";

std::vector<gnss::Sentence> feedAll(gnss::NmeaParser &p, const std::string &wire)
{
    std::vector<gnss::Sentence> out;
    p.feed(reinterpret_cast<const uint8_t *>(wire.data()), wire.size(),
           [&](const gnss::Sentence &s) { out.push_back(s); });
    return out;
}

}  // namespace

int main()
{
    {
        gnss::NmeaParser p;
        auto out = feedAll(p, kGgaFix);
        check(out.size() == 1, "valid GGA yields exactly one sentence");
        check(out.size() == 1 && out[0].talker == "GN" && out[0].type == "GGA",
              "talker/type split correctly");
        check(out.size() == 1 && out[0].fields.size() >= 9, "GGA has at least the fields the model reads");
        check(out.size() == 1 && out[0].fields[1] == "4717.11399" && out[0].fields[2] == "N",
              "lat/hemi fields land at the expected index");
    }
    {
        gnss::NmeaParser p;
        auto out = feedAll(p, kRmcFix);
        check(out.size() == 1 && out[0].type == "RMC" && out[0].fields[1] == "A", "valid RMC parses, status A");
    }
    {
        gnss::NmeaParser p;
        auto out = feedAll(p, kGsv);
        check(out.size() == 1 && out[0].type == "GSV",
              "an unrecognized-by-the-model sentence still surfaces if its checksum is valid");
    }
    {
        gnss::NmeaParser p;
        std::string corrupt = kGgaFix;   /* checksum is "45" (verified independently); flip a digit */
        size_t star = corrupt.find('*');
        corrupt[star + 1] = (corrupt[star + 1] == '0') ? '1' : '0';
        auto out = feedAll(p, corrupt);
        check(out.empty(), "a corrupted checksum is silently dropped, not surfaced");
    }
    {
        gnss::NmeaParser p;
        auto out = feedAll(p, "$GNGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,\r\n");
        check(out.empty(), "a sentence with no '*' checksum at all is dropped");
    }
    {
        gnss::NmeaParser p;
        auto out = feedAll(p, "\x01\x02\xffrandom noise, no dollar sign\r\n");
        check(out.empty(), "a line not starting with '$' is dropped, not crashed on");
    }
    {
        /* Chunk-invariant: same bytes, arbitrary split, same result — the
         * property tools/ocp_fuzz.py holds ocp::Parser to. */
        gnss::NmeaParser p;
        std::string wire = kGgaFix;
        std::vector<gnss::Sentence> out;
        for (size_t i = 0; i < wire.size(); i += 3) {
            size_t n = std::min<size_t>(3, wire.size() - i);
            p.feed(reinterpret_cast<const uint8_t *>(wire.data() + i), n,
                   [&](const gnss::Sentence &s) { out.push_back(s); });
        }
        check(out.size() == 1 && out[0].talker == "GN" && out[0].type == "GGA",
              "a sentence torn across 3-byte chunks still parses whole");
    }
    {
        gnss::NmeaParser p;
        std::string overlong(gnss::NmeaParser::kMaxLineLen + 40, 'A');
        overlong = "$" + overlong + "*00\r\n";
        std::string wire = overlong + kGgaFix;   /* recovery: a good line right after */
        auto out = feedAll(p, wire);
        check(out.size() == 1 && out[0].type == "GGA",
              "an overlong line is bounded and dropped without wedging the next one");
    }
    {
        gnss::NmeaParser p;
        std::string wire = std::string(kGgaFix) + kRmcFix + kGsv;
        auto out = feedAll(p, wire);
        check(out.size() == 3 && out[0].type == "GGA" && out[1].type == "RMC" && out[2].type == "GSV",
              "multiple sentences in one feed() come out in order");
    }
    {
        gnss::NmeaParser p;
        auto out = feedAll(p, kGgaNoFix);
        check(out.size() == 1 && out[0].fields[5] == "0", "a no-fix GGA (quality=0) still parses — it's the model's job to read that");
    }
    {
        gnss::NmeaParser p;
        auto out = feedAll(p, kRmcNoFix);
        check(out.size() == 1 && out[0].fields[1] == "V", "a no-fix RMC (status=V) still parses");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "nmea parser test FAILED" : "nmea parser test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
