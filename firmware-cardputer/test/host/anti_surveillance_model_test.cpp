/*
 * anti_surveillance_model_test.cpp — host test for deck-local movement
 * correlation. No GNSS or tracker identifier leaves this model's RAM.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cmath>
#include <cstdio>
#include <string>

#include "model/anti_surveillance_model.h"
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

ocp::Item trackerEvent(const char *mac, int rssi)
{
    return eventOf(std::string("[EVT] kind=airtag mac=") + mac +
                   " rssi=" + std::to_string(rssi) + " n=1\n");
}

}  // namespace

int main()
{
    model::AntiSurveillanceModel m;
    m.begin();
    check(m.active() && m.trackers().empty(), "begin() starts a clean active session");

    m.absorbEvent(eventOf("[EVT] kind=airtag mac=not-a-mac rssi=-71 n=1\n"), 900);
    m.absorbEvent(eventOf("[EVT] kind=airtag mac=aa:bb:cc:dd:ee:ff rssi=bad n=1\n"), 901);
    check(m.trackers().empty(), "malformed MAC and RSSI fields are ignored");

    m.observePosition(47.0, 8.0, true, 0, 1000);
    m.absorbEvent(trackerEvent("aa:bb:cc:dd:ee:ff", -71), 1100);
    m.absorbEvent(trackerEvent("aa:bb:cc:dd:ee:ff", -69), 1200);
    check(m.trackers().size() == 1 && m.trackers()[0].movement_legs == 0,
          "repeated sightings at one position do not create movement");

    /* About 33 m north per 0.0003 degrees of latitude. */
    m.observePosition(47.0003, 8.0, true, 0, 2000);
    m.absorbEvent(trackerEvent("aa:bb:cc:dd:ee:ff", -67), 2100);
    check(m.trackers()[0].movement_legs == 1 && !m.trackers()[0].alert,
          "one fresh 25m movement leg is only a candidate");

    m.observePosition(47.0006, 8.0, true, 0, 3000);
    m.absorbEvent(trackerEvent("aa:bb:cc:dd:ee:ff", -65), 3100);
    check(m.trackers()[0].movement_legs == 2 && m.trackers()[0].alert &&
          m.alertCount() == 1 && m.latestAlert() == &m.trackers()[0],
          "two separated movement legs produce one conservative alert");
    check(m.trackers()[0].sightings == 4 && m.trackers()[0].traveled_m > 50.0 &&
          m.trackers()[0].rssi == -65, "tracker counters and latest RSSI are updated");

    m.observePosition(47.0009, 8.0, false, 0, 4000);
    m.absorbEvent(trackerEvent("aa:bb:cc:dd:ee:ff", -63), 4100);
    check(m.trackers()[0].movement_legs == 2,
          "a sighting without a current fix cannot create a movement leg");

    m.stop();
    m.absorbEvent(trackerEvent("11:22:33:44:55:66", -50), 5000);
    check(!m.active() && m.trackers().size() == 1,
          "stop freezes correlation and preserves the last look");

    m.clear();
    check(!m.active() && m.trackers().empty() && m.alertCount() == 0,
          "clear removes bounded session state");

    m.begin();
    for (unsigned i = 0; i < model::AntiSurveillanceModel::kMaxTrackers; i++) {
        char mac[18];
        std::snprintf(mac, sizeof mac, "00:00:00:00:%02x:%02x", i >> 8, i & 0xff);
        m.absorbEvent(trackerEvent(mac, -80), 6000 + i);
    }
    m.absorbEvent(trackerEvent("ff:ff:ff:ff:ff:ff", -40), 7000);
    check(m.trackers().size() == model::AntiSurveillanceModel::kMaxTrackers,
          "tracker table is bounded and drops overflow");

    std::printf("\n%s: %d passed, %d failed\n",
                g_fail ? "anti-surveillance model test FAILED" : "anti-surveillance model test OK",
                g_pass, g_fail);
    return g_fail ? 1 : 0;
}
