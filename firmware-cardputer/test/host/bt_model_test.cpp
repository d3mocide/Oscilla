/*
 * bt_model_test.cpp — host test for model::BtModel.
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdio>
#include <string>

#include "model/bt_model.h"
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
        model::BtModel m;
        m.beginScan();
        check(m.scanning() && m.devices().empty(), "beginScan() starts clean and scanning");

        std::string w = "[BLE] BEGIN n=2 total=2 dwell_ms=6000 elapsed_ms=6012\n"
                        "[BLE] \"f4:12:34:56:78:9a\",\"Pixel Buds\",\"0075\",\"\",\"-58\",\"14\"\n"
                        "[BLE] \"aa:bb:cc:dd:ee:ff\",\"\",\"004c\",\"airtag\",\"-71\",\"6\"\n"
                        "[BLE] END\n";
        m.absorbScan(frameOf(w));
        check(!m.scanning(), "absorbScan() ends scanning — [BLE] has no paging, this is the whole result");
        check(m.devices().size() == 2 && m.malformedRows() == 0, "two device rows parsed");
        check(m.devices()[0].mac == "f4:12:34:56:78:9a" && m.devices()[0].name == "Pixel Buds" &&
              m.devices()[0].mfr == "0075" && !m.devices()[0].tracker, "non-tracker row fields");
        check(m.devices()[1].mac == "aa:bb:cc:dd:ee:ff" && m.devices()[1].name.empty() &&
              m.devices()[1].mfr == "004c" && m.devices()[1].tracker && m.devices()[1].rssi == -71 &&
              m.devices()[1].n == 6, "tracker row: empty name, tracker flag set from the 'airtag' column");
    }
    {
        model::BtModel m;
        m.beginScan();
        std::string w = "[BLE] BEGIN n=2 total=2\n"
                        "[BLE] \"f4:12:34:56:78:9a\",\"OK\",\"\",\"\",\"-58\",\"1\"\n"
                        "[BLE] \"bad-mac\",\"OK\",\"\",\"\",\"-58\",\"1\"\n"           /* short mac */
                        "[BLE] END\n";
        m.absorbScan(frameOf(w));
        check(m.devices().size() == 1 && m.malformedRows() == 1, "malformed rows counted and skipped, not trusted");
    }
    {
        model::BtModel m;
        m.beginAirtag();
        check(m.airtagActive() && m.trackerHits().empty() && m.trackerCount() == 0, "beginAirtag() starts clean");

        m.absorbEvent(eventOf("[EVT] kind=airtag mac=aa:bb:cc:dd:ee:ff rssi=-71 n=1\n"));
        check(m.trackerCount() == 1 && m.trackerHits().size() == 1, "first tracker sighting recorded");
        check(m.trackerHits()[0].mac == "aa:bb:cc:dd:ee:ff" && m.trackerHits()[0].rssi == -71, "sighting fields");

        m.absorbEvent(eventOf("[EVT] kind=airtag mac=aa:bb:cc:dd:ee:ff rssi=-69 n=2\n"));
        check(m.trackerCount() == 2 && m.trackerHits().size() == 2, "second sighting counted, not deduplicated");
        check(m.trackerHits()[0].rssi == -69, "most recent sighting is index 0");

        m.absorbEvent(eventOf("[EVT] kind=sniff pkts=1 ch=1\n"));
        check(m.trackerCount() == 2, "a non-airtag event kind is ignored");
    }
    {
        /* trackerHits caps at kMaxTrackerHits but the running total keeps counting. */
        model::BtModel m;
        m.beginAirtag();
        for (unsigned i = 0; i < model::BtModel::kMaxTrackerHits + 5; i++) {
            m.absorbEvent(eventOf("[EVT] kind=airtag mac=aa:bb:cc:dd:ee:ff rssi=-71 n=1\n"));
        }
        check(m.trackerHits().size() == model::BtModel::kMaxTrackerHits, "tracker hit log caps at kMaxTrackerHits");
        check(m.trackerCount() == model::BtModel::kMaxTrackerHits + 5,
              "the running total is not capped, only the kept log");
    }
    {
        model::BtModel m;
        m.beginScan();
        std::string w = "[BLE] BEGIN n=1 total=1\n"
                        "[BLE] \"f4:12:34:56:78:9a\",\"OK\",\"\",\"\",\"-58\",\"1\"\n"
                        "[BLE] END\n";
        m.absorbScan(frameOf(w));
        m.beginAirtag();
        m.absorbEvent(eventOf("[EVT] kind=airtag mac=aa:bb:cc:dd:ee:ff rssi=-71 n=1\n"));
        m.stop();
        check(!m.scanning() && !m.airtagActive() && m.devices().size() == 1 && m.trackerHits().size() == 1,
              "stop clears live state but keeps the tables");

        m.clear();
        check(m.devices().empty() && m.trackerHits().empty() && m.trackerCount() == 0,
              "probe reset clears everything");
    }

    std::printf("\n%s: %d passed, %d failed\n", g_fail ? "bt model test FAILED" : "bt model test OK",
               g_pass, g_fail);
    return g_fail ? 1 : 0;
}
