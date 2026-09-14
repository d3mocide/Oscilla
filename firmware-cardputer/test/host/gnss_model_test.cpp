/*
 * gnss_model_test.cpp — host test for model::GnssModel: fix validity/age and
 * "no fix" vs "no UART data" as distinct, independently-observable states
 * (Rev D §6, DESIGN §9.1).
 *
 * SPDX-License-Identifier: MIT
 */

#include <cmath>
#include <cstdio>
#include <string>

#include "gnss/nmea_parser.h"
#include "model/gnss_model.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

gnss::Sentence sentenceOf(const std::string &wire)
{
    gnss::NmeaParser p;
    gnss::Sentence out;
    p.feed(reinterpret_cast<const uint8_t *>(wire.data()), wire.size(),
           [&](const gnss::Sentence &s) { out = s; });
    return out;
}

bool near(double a, double b, double eps) { return std::fabs(a - b) < eps; }

const char *kGgaFix   = "$GNGGA,092725.00,4717.11399,N,00833.91590,E,1,08,1.01,499.6,M,48.0,M,,*45\r\n";
const char *kGgaNoFix = "$GNGGA,092725.00,,,,,0,00,99.99,,M,,M,,*73\r\n";
const char *kRmcFix   = "$GNRMC,092725.00,A,4717.11399,N,00833.91590,E,0.004,77.52,091202,,,A*4A\r\n";
const char *kRmcNoFix = "$GNRMC,092725.00,V,,,,,,,091202,,,N*60\r\n";
const char *kGsv      = "$GPGSV,3,1,11,10,63,137,17,07,61,098,15,05,59,290,20,08,54,157,30*70\r\n";

}  // namespace

int main()
{
    {
        model::GnssModel m;
        check(!m.hasUartData(0), "starts with no UART data at all");
        check(!m.hasFix() && !m.everFixed(), "starts with no fix, never fixed");
        check(m.fixAgeMs(1000) == 0, "fix age is 0 before any fix has ever been seen");
    }
    {
        model::GnssModel m;
        m.absorb(sentenceOf(kGgaFix), 1000);
        check(m.hasUartData(1000) && m.hasUartData(3500), "a sentence marks the link alive for its timeout window");
        check(!m.hasUartData(4001), "and not alive once that window has fully elapsed");

        check(m.hasFix() && m.everFixed(), "a fix-quality GGA sets both hasFix() and everFixed()");
        check(near(m.fix().lat_deg, 47.2852331667, 1e-6), "lat converted ddmm.mmmm -> decimal degrees correctly");
        check(near(m.fix().lon_deg, 8.56526500, 1e-6), "lon converted dddmm.mmmm -> decimal degrees correctly");
        check(near(m.fix().alt_m, 499.6, 1e-9), "altitude read from field 9");
        check(near(m.fix().hdop, 1.01, 1e-6), "hdop read from field 8");
        check(m.fix().utc == "092725.00", "utc read from field 1");
        check(m.fixAgeMs(1500) == 500, "fix age tracks elapsed time since the fix was set");
    }
    {
        /* The headline distinction: quality=0 is "no fix" while sentences
         * keep arriving — very different from silence on the wire. */
        model::GnssModel m;
        m.absorb(sentenceOf(kGgaFix), 1000);
        m.absorb(sentenceOf(kGgaNoFix), 2000);
        check(m.hasUartData(2000), "still receiving data (the no-fix sentence itself)");
        check(!m.hasFix(), "but the current instant has no fix");
        check(m.everFixed(), "the earlier fix is still remembered as having happened");
        check(near(m.fix().lat_deg, 47.2852331667, 1e-6), "last known position is kept, not zeroed, on a no-fix report");
        check(m.fixAgeMs(5000) == 4000, "fix age keeps counting from the last *valid* fix, not the no-fix report");
    }
    {
        /* The other half: real silence, never confused with "reported no fix". */
        model::GnssModel m;
        m.absorb(sentenceOf(kGgaFix), 1000);
        check(!m.hasUartData(1000 + model::GnssModel::kNoDataTimeoutMs + 1),
              "no sentence at all for the timeout window reads as no UART data");
        check(m.hasFix(), "hasFix() reflects the last report's own flag and is independent of the data-timeout check");
    }
    {
        model::GnssModel m;
        m.absorb(sentenceOf(kRmcFix), 1000);
        check(m.hasFix() && m.everFixed(), "RMC status=A also sets a fix");
        check(near(m.fix().lat_deg, 47.2852331667, 1e-6) && near(m.fix().lon_deg, 8.56526500, 1e-6),
              "RMC lat/lon convert the same way as GGA's");

        m.absorb(sentenceOf(kRmcNoFix), 2000);
        check(!m.hasFix() && m.everFixed(), "RMC status=V clears hasFix() but not everFixed()");
    }
    {
        /* GSV carries no position; must not corrupt an existing fix or
         * spuriously grant/deny one. */
        model::GnssModel m;
        m.absorb(sentenceOf(kGgaFix), 1000);
        m.absorb(sentenceOf(kGsv), 1200);
        check(m.hasUartData(1200), "GSV still counts as UART activity");
        check(m.hasFix(), "GSV doesn't touch fix validity either way");
        check(near(m.fix().lat_deg, 47.2852331667, 1e-6), "GSV doesn't touch the stored position");
    }
    {
        /* An unparsable-but-claims-valid GGA must not be trusted, per the
         * same "never trust a partial event" rule LoraModel follows. */
        model::GnssModel m;
        gnss::Sentence bad = sentenceOf(kGgaFix);
        bad.fields[1] = "not-a-number";
        m.absorb(bad, 1000);
        check(!m.everFixed(), "a fix-quality GGA with unparsable lat is not trusted as a fix");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "gnss model test FAILED" : "gnss model test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
