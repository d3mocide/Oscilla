/*
 * ook_model_test.cpp — host test for model::OokModel (OCP-SPEC §15).
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "model/ook_model.h"
#include "ocp/ocp_parser.h"

namespace {

int g_pass, g_fail;
void check(bool ok, const char *what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    (ok ? g_pass : g_fail)++;
}

ocp::Item itemOf(const std::string &wire, ocp::ItemKind want)
{
    ocp::Parser p;
    ocp::Item out;
    p.feed(reinterpret_cast<const uint8_t *>(wire.data()), wire.size(),
           [&](ocp::Item &&it) { if (it.kind == want) out = std::move(it); });
    return out;
}
ocp::Item evt(const std::string &w) { return itemOf(w + "\n", ocp::ItemKind::Event); }
ocp::Item frame(const std::string &w) { return itemOf(w + " END\n", ocp::ItemKind::Frame); }

const char *kPrn = "[EVT] kind=ook decoder=princeton nbits=24 hex=a5c3e1 frames=1 te_us=400 rssi=-41";
const char *kLax = "[EVT] kind=ook decoder=lacrosse_tx141thbv2 nbits=40 hex=c0824f3d35 frames=1 te_us=240 "
                   "rssi=-77 id=192 ch=0 batt_low=1 test=0 temp_c10=91 hum=61";

}  // namespace

int main()
{
    {   /* Princeton: the remote is its code. */
        model::OokModel m;
        m.begin();
        const auto *p = m.absorbEvent(evt(kPrn), 100);
        check(p && p->decoder == model::OokDecoder::Princeton && p->hex == "a5c3e1" && p->nbits == 24 &&
                  p->te_us == 400 && p->has_rssi && p->rssi == -41 && p->received_ms == 100,
              "princeton event parsed whole");
        check(m.devices().size() == 1 && m.devices()[0].count == 1 && m.totalPackets() == 1,
              "first press makes one device");
        m.absorbEvent(evt(kPrn), 5000);
        check(m.devices().size() == 1 && m.devices()[0].count == 2 && m.devices()[0].first_ms == 100 &&
                  m.devices()[0].last.received_ms == 5000 && m.packets().size() == 2,
              "the same code again is the same device, counted, last time updated");
        m.absorbEvent(evt("[EVT] kind=ook decoder=princeton nbits=24 hex=111111 frames=2 te_us=350"), 6000);
        check(m.devices().size() == 2 && m.devices()[0].last.hex == "111111" && !m.devices()[0].last.has_rssi,
              "a different code is a new device, newest first; missing rssi stays absent, not zero");
        m.absorbEvent(evt(kPrn), 7000);
        check(m.devices()[0].last.hex == "a5c3e1" && m.devices()[0].count == 3,
              "a device heard again moves back to the top");
    }
    {   /* LaCrosse: identity is id + channel; the reading is the latest one. */
        model::OokModel m;
        m.begin();
        const auto *p = m.absorbEvent(evt(kLax), 10);
        check(p && p->decoder == model::OokDecoder::LaCrosse && p->id == 192 && p->ch == 0 && p->batt_low &&
                  !p->test && p->temp_c10 == 91 && p->hum == 61,
              "lacrosse fields parsed");
        m.absorbEvent(evt("[EVT] kind=ook decoder=lacrosse_tx141thbv2 nbits=40 hex=c0824e3d34 frames=1 te_us=240 "
                          "id=192 ch=0 batt_low=1 test=0 temp_c10=-15 hum=60"), 60000);
        check(m.devices().size() == 1 && m.devices()[0].count == 2 && m.devices()[0].last.temp_c10 == -15 &&
                  m.devices()[0].last.hum == 60,
              "a new reading (different bytes) from the same sensor updates it; negative temperature ok");
        m.absorbEvent(evt("[EVT] kind=ook decoder=lacrosse_tx141thbv2 nbits=40 hex=c0924f3d00 frames=1 te_us=240 "
                          "id=192 ch=1 batt_low=0 test=0 temp_c10=91 hum=61"), 61000);
        check(m.devices().size() == 2, "same id on another channel is another sensor");
    }
    {   /* Malformed events never half-apply. */
        model::OokModel m;
        m.begin();
        const char *bad[] = {
            "[EVT] kind=ook decoder=keeloq nbits=66 hex=00 frames=1 te_us=400",
            "[EVT] kind=ook decoder=princeton nbits=24 hex=a5c3 frames=1 te_us=400",
            "[EVT] kind=ook decoder=princeton nbits=24 hex=A5C3E1 frames=1 te_us=400",
            "[EVT] kind=ook decoder=princeton nbits=24 hex=a5c3zz frames=1 te_us=400",
            "[EVT] kind=ook decoder=princeton nbits=23 hex=a5c3e1 frames=1 te_us=400",
            "[EVT] kind=ook decoder=princeton nbits=24 hex=a5c3e1 te_us=400",
            "[EVT] kind=ook decoder=princeton nbits=24 hex=a5c3e1 frames=1 te_us=400 rssi=12",
            "[EVT] kind=ook decoder=princeton nbits=24 hex=a5c3e1 frames=1 te_us=400 rssi=x",
            "[EVT] kind=ook decoder=lacrosse_tx141thbv2 nbits=40 hex=c0824f3d35 frames=1 te_us=240 "
            "id=192 ch=0 batt_low=1 test=0 hum=61",
            "[EVT] kind=ook decoder=lacrosse_tx141thbv2 nbits=40 hex=c0824f3d35 frames=1 te_us=240 "
            "id=192 ch=4 batt_low=1 test=0 temp_c10=91 hum=61",
            "[EVT] kind=ook decoder=lacrosse_tx141thbv2 nbits=40 hex=c0824f3d35 frames=1 te_us=240 "
            "id=192 ch=0 batt_low=1 test=0 temp_c10=3596 hum=61",
            "[EVT] kind=ook_unknown n=0 pulses=66 min_us=160 max_us=1240 dur_ms=47",
            "[EVT] kind=ook_unknown n=3 pulses=66 min_us=1300 max_us=1240 dur_ms=47",
        };
        int accepted = 0;
        for (const char *b : bad) accepted += m.absorbEvent(evt(b), 1) != nullptr;
        check(accepted == 0 && m.devices().empty() && m.packets().empty() && m.totalPackets() == 0 &&
                  m.unknownTrains() == 0 && !m.lastUnknown().valid,
              "13 malformed events: none stored, no counter moved");
        check(!m.absorbEvent(evt("[EVT] kind=lora rssi=-67 snr=8 len=2 hex=aabb"), 1),
              "other kinds are not ours");
    }
    {   /* Unknown summaries: a counter and the latest shape only. */
        model::OokModel m;
        m.begin();
        check(!m.absorbEvent(evt("[EVT] kind=ook_unknown n=3 pulses=66 min_us=160 max_us=1240 dur_ms=47 rssi=-63"), 9),
              "ook_unknown returns no packet");
        m.absorbEvent(evt("[EVT] kind=ook_unknown n=2 pulses=12 min_us=200 max_us=800 dur_ms=5"), 20);
        check(m.unknownTrains() == 5 && m.lastUnknown().pulses == 12 && !m.lastUnknown().has_rssi &&
                  m.lastUnknown().received_ms == 20 && m.devices().empty(),
              "summaries add their covered count and keep only the latest shape");
    }
    {   /* [OOK] replies: listen vs status. */
        model::OokModel m;
        check(!m.absorbReply(frame("[OOK] running=1 freq_khz=433920 baud=24993")) && m.freqKhz() == 433920 &&
                  m.baud() == 24993 && !m.health().valid,
              "listen reply: frequency and baud, no health");
        check(m.absorbReply(frame("[OOK] running=1 freq_khz=433920 baud=24993 trains=52 packets=10 unknown=1 "
                                  "overflows=0 dropped=0 errors=0 reinit=2 hw_fault=0 ocp_drop=0")) &&
                  m.health().valid && m.health().trains == 52 && m.health().reinit == 2,
              "status reply: complete health");
        check(!m.absorbReply(frame("[OOK] running=1 freq_khz=433920 baud=24993 trains=53 packets=10 unknown=1 "
                                   "overflows=0 dropped=0 errors=0 reinit=2 hw_fault=0")) &&
                  m.health().trains == 52,
              "a partial counter set leaves the last good health untouched");
        m.begin();
        check(m.active() && !m.health().valid && m.freqKhz() == 433920, "begin() resets the session, keeps tuning");
        m.absorbReply(frame("[OOK] running=0 freq_khz=433920 baud=24993 trains=0 packets=0 unknown=0 overflows=0 "
                            "dropped=0 errors=0 reinit=0 hw_fault=0 ocp_drop=0"));
        check(!m.active(), "running=0 in a reply means the lane is not live");
        check(!m.absorbReply(frame("[LORA] running=1")), "other markers are not ours");
    }
    {   /* Caps. */
        model::OokModel m;
        m.begin();
        for (int i = 0; i < 40; ++i) {
            char line[160];
            std::snprintf(line, sizeof line,
                          "[EVT] kind=ook decoder=princeton nbits=24 hex=%06x frames=1 te_us=400", 0x100000 + i);
            m.absorbEvent(evt(line), static_cast<uint32_t>(i));
        }
        check(m.packets().size() == model::OokModel::kMaxPackets && m.packets()[0].hex == "100027",
              "packet log capped, newest first");
        check(m.devices().size() == model::OokModel::kMaxDevices && m.devices()[0].last.hex == "100027" &&
                  m.devices().back().last.hex == "100018",
              "device table capped; the least recently heard go first");
        check(m.totalPackets() == 40, "total keeps counting past the caps");
        m.clear();
        check(!m.active() && m.devices().empty() && m.freqKhz() == 0, "clear() forgets everything");
    }

    std::printf("ook model test %s: %d passed, %d failed\n", g_fail ? "FAILED" : "OK", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
