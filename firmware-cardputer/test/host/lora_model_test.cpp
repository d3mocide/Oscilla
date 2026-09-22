/*
 * lora_model_test.cpp — host test for model::LoraModel.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "model/lora_model.h"
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
        model::LoraModel m;
        check(!m.hasConfig() && !m.active(), "starts unconfigured and inactive");
        m.configured(910525000, 7, 62, 1, "meshcore_us_ca");
        check(m.hasConfig() && m.freqHz() == 910525000 && m.sf() == 7 && m.bwKhz() == 62 && m.cr() == 1 &&
              m.profile() == "meshcore_us_ca",
              "configured() records what the deck sent, not parsed from a reply");

        m.begin();
        check(m.active() && m.packets().empty() && m.totalCount() == 0, "begin() starts clean");

        m.absorbEvent(eventOf("[EVT] kind=lora rssi=-67 snr=12.0 len=10 hex=0102030405060708090a\n"));
        check(m.packets().size() == 1 && m.totalCount() == 1, "first packet recorded");
        check(m.packets()[0].rssi == -67 && m.packets()[0].snr == 12.0f && m.packets()[0].len == 10 &&
              m.packets()[0].hex == "0102030405060708090a", "fields parsed");

        m.absorbEvent(eventOf("[EVT] kind=lora rssi=-70 snr=8.5 len=2 hex=aabb\n"));
        check(m.packets().size() == 2 && m.totalCount() == 2, "second packet appended");
        check(m.packets()[0].hex == "aabb" && m.packets()[1].hex == "0102030405060708090a", "newest first");
    }
    {
        model::LoraModel m;
        m.configured(915000000, 7, 125, 1, "manual");
        m.begin();
        m.absorbEvent(eventOf("[EVT] kind=chan ch=1 pkts=1\n"));
        check(m.packets().empty(), "a different event kind is ignored");
        m.absorbEvent(eventOf("[EVT] kind=lora rssi=-67 snr=12.0 len=10 hex=aabb\n"));
        check(m.packets().empty(), "hex length not matching len is dropped, not trusted");
        m.absorbEvent(eventOf("[EVT] kind=lora rssi=-67 len=2 hex=aabb\n"));
        check(m.packets().empty(), "missing snr is dropped");
    }
    {
        model::LoraModel m;
        m.configured(915000000, 7, 125, 1, "manual");
        m.begin();
        for (int i = 0; i < 40; i++) {
            m.absorbEvent(eventOf("[EVT] kind=lora rssi=-67 snr=12.0 len=2 hex=aabb\n"));
        }
        check(m.packets().size() == model::LoraModel::kMaxRows, "log capped, oldest dropped");
        check(m.totalCount() == 40, "total count keeps counting past the log cap");
    }
    {
        model::LoraModel m;
        m.configured(915000000, 7, 125, 1, "manual");
        m.begin();
        m.absorbEvent(eventOf("[EVT] kind=lora rssi=-67 snr=12.0 len=2 hex=aabb\n"));
        m.stop();
        check(!m.active() && m.packets().size() == 1, "stop clears live state but keeps the log");
        m.clear();
        check(!m.active() && !m.hasConfig() && m.packets().empty() && m.totalCount() == 0,
              "probe reset clears everything, including config");
    }
    {
        model::LoraModel m;
        check(!m.absorbStatus(eventOf("[LORA] running=1 rx=3 crc_err=1 header_err=2 irq_drop=4 radio_drop=5 ocp_drop=6 END\n")),
              "status parser accepts replies, not events");
        ocp::Parser p;
        ocp::Item reply;
        const std::string wire = "[LORA] running=1 rx=3 crc_err=1 header_err=2 irq_drop=4 radio_drop=5 ocp_drop=6 END\n";
        p.feed(reinterpret_cast<const uint8_t *>(wire.data()), wire.size(),
               [&](ocp::Item &&it) { reply = std::move(it); });
        check(m.absorbStatus(reply) && m.health().valid && m.health().rx == 3 && m.health().crc_err == 1 &&
              m.health().header_err == 2 && m.health().irq_drop == 4 && m.health().radio_drop == 5 && m.health().ocp_drop == 6,
              "complete C5 health snapshot is parsed as one unit");
        const std::string partial_wire = "[LORA] running=1 rx=4 crc_err=1 header_err=2 irq_drop=4 radio_drop=5 END\n";
        ocp::Parser partial_parser;
        partial_parser.feed(reinterpret_cast<const uint8_t *>(partial_wire.data()), partial_wire.size(),
                            [&](ocp::Item &&it) { reply = std::move(it); });
        check(!m.absorbStatus(reply) && m.health().rx == 3, "partial health reply cannot overwrite the last complete snapshot");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "lora model test FAILED" : "lora model test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
