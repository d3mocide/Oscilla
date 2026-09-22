/*
 * legacy_model_test.cpp — host test for model::LegacyModel.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "model/legacy_model.h"
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

ocp::Item replyOf(const std::string &wire)
{
    ocp::Parser p;
    ocp::Item out;
    p.feed(reinterpret_cast<const uint8_t *>(wire.data()), wire.size(),
           [&](ocp::Item &&it) { if (it.kind == ocp::ItemKind::Frame) out = std::move(it); });
    return out;
}

}  // namespace

int main()
{
    {
        model::LegacyModel m;
        check(!m.hasConfig() && !m.active(), "starts unconfigured and inactive");
        m.configured(433920000);
        check(m.hasConfig() && m.freqHz() == 433920000,
              "configured() records what the deck sent, not parsed from a reply");

        m.begin();
        check(m.active() && m.chunks().empty() && m.total() == 0, "begin() starts clean");

        m.absorbEvent(eventOf("[EVT] kind=legacy rssi=-63 len=5 hex=0102030405\n"));
        check(m.chunks().size() == 1 && m.total() == 1, "first chunk recorded");
        check(m.chunks()[0].rssi == -63 && m.chunks()[0].len == 5 &&
              m.chunks()[0].hex == "0102030405", "fields parsed");
        check(m.lastRssi() == -63, "lastRssi reflects the newest chunk");

        m.absorbEvent(eventOf("[EVT] kind=legacy rssi=-70 len=2 hex=aabb\n"));
        check(m.chunks().size() == 2 && m.total() == 2, "second chunk appended");
        check(m.chunks()[0].hex == "aabb" && m.chunks()[1].hex == "0102030405", "newest first");
        check(m.lastRssi() == -70, "lastRssi updates to the newest chunk");
        check(m.malformedCount() == 0, "no malformed events in this run");
    }
    {
        model::LegacyModel m;
        m.configured(433920000);
        m.begin();
        m.absorbEvent(eventOf("[EVT] kind=lora rssi=-67 snr=12.0 len=10 hex=aabb\n"));
        check(m.chunks().empty() && m.malformedCount() == 0, "a different event kind is ignored, not counted as malformed");
        m.absorbEvent(eventOf("[EVT] kind=legacy rssi=-67 len=10 hex=aabb\n"));
        check(m.chunks().empty(), "hex length not matching len is dropped, not trusted");
        check(m.malformedCount() == 1, "the malformed-but-ours event is counted");
        m.absorbEvent(eventOf("[EVT] kind=legacy len=2 hex=aabb\n"));
        check(m.chunks().size() == 1, "missing rssi still records (defaults to 0), unlike LoRa's missing snr");
        check(m.malformedCount() == 1, "a well-formed event afterward doesn't add to the malformed count");
    }
    {
        model::LegacyModel m;
        m.configured(433920000);
        m.begin();
        for (int i = 0; i < 40; i++) {
            m.absorbEvent(eventOf("[EVT] kind=legacy rssi=-67 len=2 hex=aabb\n"));
        }
        check(m.chunks().size() == model::LegacyModel::kMaxRows, "log capped, oldest dropped");
        check(m.total() == 40, "total keeps counting past the log cap");
    }
    {
        model::LegacyModel m;
        m.configured(433920000);
        m.begin();
        m.absorbEvent(eventOf("[EVT] kind=legacy rssi=-67 len=2 hex=aabb\n"));
        m.stop();
        check(!m.active() && m.chunks().size() == 1, "stop clears live state but keeps the log");
        m.clear();
        check(!m.active() && !m.hasConfig() && m.chunks().empty() && m.total() == 0,
              "probe reset clears everything, including config");
    }
    {
        model::LegacyModel m;
        check(m.fifoOverflows() == 0 && m.queueDrops() == 0, "counters start at zero");
        m.absorbStatus(replyOf("[LEGACY] running=1 freq=433920000 overflow=3 qdrops=7 END\n"));
        check(m.fifoOverflows() == 3 && m.queueDrops() == 7, "absorbStatus reads overflow=/qdrops= from a status reply");
        m.absorbStatus(replyOf("[LEGACY] running=0 configured=0 END\n"));
        check(m.fifoOverflows() == 3 && m.queueDrops() == 7,
              "a reply without overflow=/qdrops= leaves the last known values alone, doesn't reset to zero");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "legacy model test FAILED" : "legacy model test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
