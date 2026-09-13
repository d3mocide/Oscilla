/*
 * model_test.cpp — host test for model::ScanModel.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "model/scan_model.h"
#include "ocp/ocp_parser.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

ocp::Item frameOf(const std::string &wire)
{
    ocp::Parser p;
    ocp::Item out;
    p.feed(reinterpret_cast<const uint8_t *>(wire.data()), wire.size(),
           [&](ocp::Item &&it) { if (it.kind == ocp::ItemKind::Frame) out = std::move(it); });
    return out;
}

std::string row(unsigned idx, int rssi = -50, unsigned ch = 6)
{
    char b[160];
    std::snprintf(b, sizeof b, "[SCAN] \"%u\",\"net\\x0a%u\",\"aa:bb:cc:dd:ee:%02x\",\"%u\",\"WPA2\",\"%d\",\"%s\"\n",
                  idx, idx, idx & 0xff, ch, rssi, ch <= 14 ? "2.4" : "5");
    return b;
}

std::string page(unsigned total, unsigned first, unsigned n, const char *extra = "")
{
    std::string w = "[SCAN] BEGIN n=" + std::to_string(n) + " total=" + std::to_string(total) +
                    " first=" + std::to_string(first) + " elapsed_ms=10500" + extra + "\n";
    for (unsigned i = 0; i < n; i++) w += row(first + i);
    return w + "[SCAN] END\n";
}

}  // namespace

int main()
{
    {
        model::ScanModel m;
        m.begin();
        uint16_t next = m.absorbPage(frameOf(page(3, 1, 3)));
        check(next == 0 && !m.scanning() && m.rows().size() == 3, "single page completes");
        check(m.rows()[0].ssid == std::string("net\n1"), "SSID bytes decoded, not display-escaped");
        check(m.rows()[1].ch == 6 && !m.rows()[1].band5 && m.rows()[1].rssi == -50, "fields parsed");
        check(m.elapsedMs() == 10500 && m.total() == 3 && !m.truncated(), "totals");
    }
    {
        model::ScanModel m;
        m.begin();
        uint16_t next = m.absorbPage(frameOf(page(300, 1, 256)));
        check(next == 257 && m.scanning(), "first of two pages asks for 257");
        check(m.absorbPage(frameOf(page(300, 1, 256))) == 0 && m.rows().size() == 256,
              "a repeated or stale page is ignored");
        next = m.absorbPage(frameOf(page(300, 257, 44)));
        check(next == 0 && !m.scanning() && m.rows().size() == 300, "second page completes the survey");
        check(m.rows().back().idx == 300, "indices continue across pages");
    }
    {
        model::ScanModel m;
        m.begin();
        m.absorbPage(frameOf(page(0, 1, 0, " aborted=1")));
        check(m.aborted() && !m.scanning() && m.rows().empty(), "aborted scan");
    }
    {
        model::ScanModel m;
        m.begin();
        std::string w = "[SCAN] BEGIN n=4 total=4 first=1\n" + row(1) +
                        "[SCAN] \"2\",\"x\",\"aa:bb\",\"6\",\"WPA2\",\"-50\",\"2.4\"\n" +     /* short bssid */
                        "[SCAN] \"3\",\"x\",\"aa:bb:cc:dd:ee:03\",\"999\",\"WPA2\",\"-50\",\"2.4\"\n" + /* bad ch */
                        row(4) + "[SCAN] END\n";
        check(m.absorbPage(frameOf(w)) == 0 && m.rows().size() == 2 && m.malformedRows() == 2,
              "malformed rows are counted and skipped, not trusted");
        check(m.rows()[1].idx == 4, "valid rows around them survive");
    }
    {
        model::ScanModel m;
        m.begin();
        std::string w = "[SCAN] BEGIN n=2 total=2 first=1\n" + row(2) + row(1) + "[SCAN] END\n";
        m.absorbPage(frameOf(w));
        check(m.rows().empty() && m.malformedRows() == 2, "rows whose idx is out of place are rejected");
    }
    {
        model::ScanModel m;
        m.begin();
        uint16_t next = 1;
        unsigned total = 600;
        while (next) {
            unsigned n = total - next + 1 > 256 ? 256 : total - next + 1;
            next = m.absorbPage(frameOf(page(total, next, n)));
        }
        check(m.rows().size() == model::ScanModel::kMaxRows && m.truncated(), "capped at kMaxRows, and says so");
    }
    {
        model::ScanModel m;
        m.absorbInspect(frameOf("[INSPECT] BEGIN idx=3 bssid=aa:bb:cc:dd:ee:03 ch=36 band=5\n"
                                "[INSPECT] beacons=3 rssi=-61 rsn=1 mfp_capable=1 mfp_required=1 uptime_s=5000000000 interval_ms=102\n"
                                "[INSPECT] END\n"));
        const auto &in = m.inspect();
        check(in.valid && in.idx == 3 && in.ch == 36 && in.beacons == 3 && in.rssi == -61, "inspect parsed");
        check(in.rsn && in.mfp_capable && in.mfp_required && in.interval_ms == 102, "MFP flags");
        check(in.uptime_s == 5000000000ULL, "uptime beyond 32 bits survives");

        m.absorbInspect(frameOf("[INSPECT] BEGIN idx=1 bssid=aa:bb:cc:dd:ee:01 ch=6 band=2.4 aborted=1\n"
                                "[INSPECT] beacons=0\n[INSPECT] END\n"));
        check(m.inspect().aborted && m.inspect().beacons == 0 && !m.inspect().rsn, "aborted inspect, no beacons");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "model test FAILED" : "model test OK", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
