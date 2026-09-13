/*
 * deauth_model_test.cpp — host test for model::DeauthModel.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "model/deauth_model.h"
#include "ocp/ocp_parser.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

ocp::Item eventOf(const std::string &wire)
{
    ocp::Parser p;
    ocp::Item out;
    p.feed(reinterpret_cast<const uint8_t *>(wire.data()), wire.size(),
           [&](ocp::Item &&it) { if (it.kind == ocp::ItemKind::Event) out = std::move(it); });
    return out;
}

}  // namespace

int main()
{
    {
        model::DeauthModel m;
        m.begin();
        check(m.active() && m.events().empty() && m.totalCount() == 0, "begin() starts clean");

        m.absorbEvent(eventOf(
            "[EVT] kind=deauth bssid=aa:bb:cc:dd:ee:01 mac=f4:12:34:56:78:9a reason=7 disassoc=0 rssi=-58 ch=6 n=1\n"));
        check(m.events().size() == 1 && m.totalCount() == 1, "first detection recorded");
        check(m.events()[0].bssid == "aa:bb:cc:dd:ee:01" && m.events()[0].mac == "f4:12:34:56:78:9a" &&
              m.events()[0].reason == 7 && !m.events()[0].disassoc && m.events()[0].rssi == -58 &&
              m.events()[0].ch == 6, "fields parsed");

        m.absorbEvent(eventOf(
            "[EVT] kind=deauth bssid=aa:bb:cc:dd:ee:01 mac=f4:12:34:56:78:9a reason=7 disassoc=1 rssi=-59 ch=6 n=2\n"));
        check(m.events().size() == 2 && m.totalCount() == 2, "second detection appended, not deduped");
        check(m.events()[0].disassoc && !m.events()[1].disassoc, "newest first");
    }
    {
        model::DeauthModel m;
        m.begin();
        m.absorbEvent(eventOf("[EVT] kind=sniff pkts=1 ch=1\n"));
        check(m.events().empty(), "a different event kind is ignored");
        m.absorbEvent(eventOf("[EVT] kind=deauth bssid=aa:bb reason=7 rssi=-58 ch=6 n=1\n"));
        check(m.events().empty(), "malformed bssid (not a MAC) is dropped, not trusted");
    }
    {
        model::DeauthModel m;
        m.begin();
        for (int i = 0; i < 70; i++) {
            m.absorbEvent(eventOf(
                "[EVT] kind=deauth bssid=aa:bb:cc:dd:ee:01 mac=f4:12:34:56:78:9a reason=7 disassoc=0 rssi=-58 ch=6 n=" +
                std::to_string(i + 1) + "\n"));
        }
        check(m.events().size() == model::DeauthModel::kMaxRows, "log capped, oldest dropped");
        check(m.totalCount() == 70, "total count keeps counting past the log cap");
    }
    {
        model::DeauthModel m;
        m.begin();
        m.absorbEvent(eventOf(
            "[EVT] kind=deauth bssid=aa:bb:cc:dd:ee:01 mac=f4:12:34:56:78:9a reason=7 disassoc=0 rssi=-58 ch=6 n=1\n"));
        m.stop();
        check(!m.active() && m.events().size() == 1, "stop clears live state but keeps the log");
        m.clear();
        check(!m.active() && m.events().empty() && m.totalCount() == 0, "probe reset clears everything");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "deauth model test FAILED" : "deauth model test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
