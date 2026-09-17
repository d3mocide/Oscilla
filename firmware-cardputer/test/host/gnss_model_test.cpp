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
/* Status=V, a *different* date (2026-06-15) than kRmcFix/kRmcNoFix's shared
 * 091202 - proves the date parses independent of fix status, not just
 * incidentally alongside it. */
const char *kRmcNoFixDate2 = "$GNRMC,101530.00,V,,,,,,,150626,,,N*63\r\n";
/* Status=A but an unparsable date field. */
const char *kRmcBadDate = "$GNRMC,101530.00,A,4717.11399,N,00833.91590,E,0.004,77.52,zzzzzz,,,A*4F\r\n";

}  // namespace

int main()
{
    {
        model::GnssModel m;
        check(!m.hasUartData(0), "starts with no UART data at all");
        check(!m.hasFix() && !m.everFixed(), "starts with no fix, never fixed");
        check(m.fixAgeMs(1000) == 0, "fix age is 0 before any fix has ever been seen");
        check(m.fix().year == 0, "no date until an RMC sentence sets one");
    }
    {
        // GGA carries time but no date field at all - must never touch it.
        model::GnssModel m;
        m.absorb(sentenceOf(kGgaFix), 1000);
        check(m.hasFix() && m.fix().year == 0, "a GGA-only session has a fix but still no date - GGA has none to give");
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
        check(m.fix().year == 2002 && m.fix().month == 12 && m.fix().day == 9,
              "RMC date field (091202) parses to 2002-12-09 (2-digit year -> 2000+yy)");

        m.absorb(sentenceOf(kRmcNoFix), 2000);
        check(!m.hasFix() && m.everFixed(), "RMC status=V clears hasFix() but not everFixed()");
    }
    {
        // Date parses independent of fix status: a receiver's clock is
        // commonly RTC-backed and keeps a real date even with no fix.
        model::GnssModel m;
        m.absorb(sentenceOf(kRmcNoFixDate2), 1000);
        check(!m.hasFix(), "status=V: no fix, as expected");
        check(m.fix().year == 2026 && m.fix().month == 6 && m.fix().day == 15,
              "but the date (150626 -> 2026-06-15) still parses despite status=V");
    }
    {
        // A malformed date must not corrupt the last known one.
        model::GnssModel m;
        m.absorb(sentenceOf(kRmcFix), 1000);   // sets 2002-12-09
        m.absorb(sentenceOf(kRmcBadDate), 2000);
        check(m.fix().year == 2002 && m.fix().month == 12 && m.fix().day == 9,
              "an unparsable date field ('zzzzzz') leaves the last known date untouched");
        check(m.hasFix(), "the rest of that same sentence (a valid fix) is unaffected by the bad date field");
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
        check(!m.everFixed() && !m.hasFix(), "a fix-quality GGA with unparsable lat is not trusted as a fix");
    }
    {
        model::GnssModel m;
        m.absorb(sentenceOf(kGgaFix), 1000);
        gnss::Sentence bad = sentenceOf(kGgaFix);
        bad.fields[1] = "9010.00000";
        m.absorb(bad, 2000);
        check(!m.hasFix(), "latitude beyond 90 degrees invalidates the current fix");
        check(near(m.fix().lat_deg, 47.2852331667, 1e-6), "impossible coordinates do not overwrite the last known position");

        bad = sentenceOf(kGgaFix);
        bad.fields[8] = "nan";
        bad.fields[7] = "inf";
        m.absorb(bad, 3000);
        check(std::isfinite(m.fix().alt_m) && std::isfinite(m.fix().hdop),
              "non-finite altitude and HDOP never enter the fix");
        check(near(m.fix().alt_m, 499.6, 1e-9) && near(m.fix().hdop, 1.01, 1e-6),
              "non-finite telemetry leaves the last finite values intact");

        m.absorb(sentenceOf(kRmcFix), 3500);
        bad = sentenceOf(kRmcFix);
        bad.fields[8] = "310226";
        m.absorb(bad, 4000);
        check(m.fix().year == 2002 && m.fix().month == 12 && m.fix().day == 9,
              "impossible calendar dates do not overwrite the last known date");
    }

    {
        /* The four states P4's exit gate has to tell apart. A view that
         * collapses any pair of these makes the antenna-unplug demo prove
         * nothing, so they're pinned here. */
        using S = model::GnssState;
        model::GnssModel m;
        check(m.state(0) == S::NoUartData, "before any sentence: no data, not 'searching'");

        m.absorb(sentenceOf(kGsv), 1000);
        check(m.state(1000) == S::Searching, "receiver talking, never fixed: searching");

        m.absorb(sentenceOf(kGgaFix), 2000);
        check(m.state(2000) == S::Fixed, "valid GGA: fixed");

        /* Antenna unplugged: the receiver keeps talking, the fix goes away.
         * This is the exit gate's own test, as a unit test. */
        m.absorb(sentenceOf(kGgaNoFix), 3000);
        check(m.state(3000) == S::FixLost, "fix lost is distinct from searching");
        check(m.hasUartData(3000), "...and the UART is still alive through it");

        /* Cable pulled: silence outranks whatever the last report said. */
        check(m.state(3000 + model::GnssModel::kNoDataTimeoutMs) == S::NoUartData,
              "silence outranks a remembered fix");

        /* A stale *valid* flag must not survive the link either. */
        model::GnssModel m2;
        m2.absorb(sentenceOf(kGgaFix), 1000);
        check(m2.state(1000) == S::Fixed, "valid fix while talking");
        check(m2.state(1000 + model::GnssModel::kNoDataTimeoutMs) == S::NoUartData,
              "a valid fix does not mask a dead UART");

        check(std::string(model::gnssStateName(S::NoUartData)) != model::gnssStateName(S::Searching)
              && std::string(model::gnssStateName(S::FixLost)) != model::gnssStateName(S::Fixed),
              "every state renders as a distinct label");
    }

    {
        /* UTC field splitting: feeds the wardrive CSV's time columns, so a
         * silent wrong answer here becomes wrong timestamps in exported data. */
        int h = -1, m = -1, sec = -1;
        check(model::splitUtcTime("092725.00", &h, &m, &sec) && h == 9 && m == 27 && sec == 25,
              "hhmmss.ss splits into 9/27/25");
        check(model::splitUtcTime("235959", &h, &m, &sec) && h == 23 && m == 59 && sec == 59,
              "the fractional part is optional");
        check(model::splitUtcTime("000000.00", &h, &m, &sec) && h == 0 && m == 0 && sec == 0,
              "midnight is a real time, not a failure");

        check(!model::splitUtcTime("0927", &h, &m, &sec) && h == 0 && m == 0 && sec == 0,
              "a short field fails and yields 0/0/0, not a partial parse");
        check(!model::splitUtcTime("", &h, &m, &sec), "an empty field fails");
        check(!model::splitUtcTime("09:7:5.00", &h, &m, &sec),
              "punctuation in the digits fails rather than parsing around it");
        check(!model::splitUtcTime("09272x.00", &h, &m, &sec),
              "a non-digit anywhere in the six fails");
        check(!model::splitUtcTime("246000.00", &h, &m, &sec), "UTC hour 24 is rejected");
        check(!model::splitUtcTime("126000.00", &h, &m, &sec), "UTC minute 60 is rejected");
        check(!model::splitUtcTime("125960.00", &h, &m, &sec), "UTC second 60 is rejected");
        check(!model::splitUtcTime("092725x00", &h, &m, &sec), "UTC suffix without a decimal point is rejected");
        check(!model::splitUtcTime("092725.", &h, &m, &sec), "UTC decimal point without fractional digits is rejected");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "gnss model test FAILED" : "gnss model test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
