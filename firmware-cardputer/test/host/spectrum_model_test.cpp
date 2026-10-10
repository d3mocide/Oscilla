/*
 * spectrum_model_test.cpp — host test for model::SpectrumModel.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "model/spectrum_model.h"
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
        model::SpectrumModel m; m.begin();
        for (int ch = 1; ch <= 60; ++ch)
            m.absorbEvent(eventOf("[EVT] kind=chan ch="+std::to_string(ch)+" pkts=1\n"), 100);
        check(m.readings().size() == 40, "hostile channel stream cannot grow history without bound");
        m.absorbEvent(eventOf("[EVT] kind=chan ch=1 pkts=7\n"), 200);
        check(m.readings()[0].pkts == 7, "capacity limit still permits known-channel updates");
    }
    {
        model::SpectrumModel m;
        m.beginLocked(6);
        for (uint32_t i = 0; i < 40; ++i)
            m.absorbEvent(eventOf("[EVT] kind=chan ch=6 pkts=" + std::to_string(i) + "\n"), i * 1000);
        const auto &r = m.readings()[0];
        check(r.history_size == 32, "packet history remains bounded");
        check(r.history[(r.history_next + 31) % 32].pkts == 39,
              "history retains newest measured count");
        const auto head = r.history_next;
        m.absorbEvent(eventOf("[EVT] kind=chan ch=11 pkts=999\n"), 50000);
        check(m.readings()[0].history_next == head, "wrong channel cannot add waterfall history");
    }
    {
        model::SpectrumModel m;
        m.beginLocked(6);
        check(!m.readings()[0].valid, "seed zero is not a received measurement");
        m.absorbEvent(eventOf("[EVT] kind=chan ch=11 pkts=999\n"), 100);
        check(!m.readings()[0].valid, "wrong-channel event cannot populate locked inspector");
        m.absorbEvent(eventOf("[EVT] kind=chan ch=6 pkts=0\n"), 123);
        check(m.readings()[0].valid && m.readings()[0].received_ms == 123,
              "real zero is valid and timestamped");
        m.absorbEvent(eventOf("[EVT] kind=chan ch=6 pkts=bad\n"), 456);
        check(m.readings()[0].received_ms == 123, "malformed event cannot refresh age");
        m.stop();
        check(!m.active() && m.readings()[0].valid, "stop preserves cached measurement");
    }
    {
        model::SpectrumModel m;
        m.begin();
        check(m.active() && !m.locked() && m.readings().empty(), "begin() starts broad and empty");

        m.absorbEvent(eventOf("[EVT] kind=chan ch=1 pkts=42\n"));
        m.absorbEvent(eventOf("[EVT] kind=chan ch=6 pkts=3\n"));
        check(m.readings().size() == 2, "broad mode accumulates distinct channels");
        check(m.readings()[0].ch == 1 && m.readings()[0].pkts == 42 && !m.readings()[0].band5, "2.4 GHz reading");

        m.absorbEvent(eventOf("[EVT] kind=chan ch=1 pkts=50\n"));
        check(m.readings().size() == 2 && m.readings()[0].pkts == 50, "repeat channel updates in place, doesn't duplicate");

        m.absorbEvent(eventOf("[EVT] kind=chan ch=157 pkts=7\n"));
        check(m.readings().size() == 3 && m.readings().back().band5, "5 GHz channel classified correctly");
    }
    {
        model::SpectrumModel m;
        m.beginLocked(6);
        check(m.active() && m.locked() && m.lockedChannel() == 6, "beginLocked() starts locked to the given channel");
        check(m.readings().size() == 1 && m.readings()[0].ch == 6 && m.readings()[0].pkts == 0,
              "locked mode seeds a single zeroed reading");

        m.absorbEvent(eventOf("[EVT] kind=chan ch=6 pkts=118\n"));
        check(m.readings().size() == 1 && m.readings()[0].pkts == 118, "locked mode updates the single reading");
        m.absorbEvent(eventOf("[EVT] kind=chan ch=6 pkts=94\n"));
        check(m.readings().size() == 1 && m.readings()[0].pkts == 94, "still one reading after further updates");
    }
    {
        model::SpectrumModel m;
        m.begin();
        m.absorbEvent(eventOf("[EVT] kind=sniff pkts=42 ch=1\n"));
        check(m.readings().empty(), "a different event kind is ignored, not misread as a channel reading");
        m.absorbEvent(eventOf("[EVT] kind=chan ch=1\n"));
        check(m.readings().empty(), "an event missing pkts is dropped, not trusted with a bogus value");
    }
    {
        model::SpectrumModel m;
        m.beginLocked(6);
        m.absorbEvent(eventOf("[EVT] kind=chan ch=6 pkts=10\n"));
        m.clear();
        check(!m.active() && !m.locked() && m.readings().empty(), "probe reset clears everything");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "spectrum model test FAILED" : "spectrum model test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
