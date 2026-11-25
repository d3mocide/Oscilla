/* Host tests for model::BaselineWatch (WP-11). */
#include "model/baseline_watch.h"
#include "ocp.h"

#include <cassert>
#include <cstdio>
#include <iostream>

namespace {

ocp::Item scanFrame(std::vector<std::string> rows)
{
    ocp::Item it;
    it.kind = ocp::ItemKind::Frame;
    it.tag = OCP_MARK_SCAN;
    it.kv = {{OCP_K_FIRST, "1"}, {OCP_K_TOTAL, std::to_string(rows.size())}};
    it.rows = std::move(rows);
    return it;
}

ocp::Item netEvent(const std::string &ssid, const std::string &bssid, int ch,
                   bool priv, bool rsn, bool mfp_cap, bool mfp_req)
{
    ocp::Item it;
    it.kind = ocp::ItemKind::Event;
    it.kv = {{OCP_K_KIND, OCP_EVT_KIND_NETWORK},
             {OCP_K_SSID, ssid},
             {OCP_K_BSSID, bssid},
             {OCP_K_CH, std::to_string(ch)},
             {OCP_K_RSSI, "-58"},
             {OCP_K_BAND, OCP_BAND_LABEL_24},
             {OCP_K_PRIVACY, priv ? "1" : "0"},
             {OCP_K_RSN, rsn ? "1" : "0"},
             {OCP_K_MFP_CAPABLE, mfp_cap ? "1" : "0"},
             {OCP_K_MFP_REQUIRED, mfp_req ? "1" : "0"},
             {OCP_K_INTERVAL_MS, "100"}};
    return it;
}

ocp::Item zigFrame(std::vector<std::string> rows)
{
    ocp::Item it;
    it.kind = ocp::ItemKind::Frame;
    it.tag = OCP_MARK_ZIG;
    it.rows = std::move(rows);
    return it;
}

}  // namespace

int main()
{
    model::ScanModel scan;
    model::ZigModel zig;

    // Seed baseline: one WPA3/MFP-required corporate AP, one hidden AP, one 802.15.4 PAN.
    scan.beginContinuous();
    scan.absorbEvent(netEvent("CorpNet,Lab\n1", "02:00:00:00:00:01", 6, true, true, true, true), 1000);
    scan.absorbEvent(netEvent("", "02:00:00:00:00:02", 11, true, true, false, false), 1000);
    zig.begin();
    zig.absorb(zigFrame({"\"pan\",\"1a2b\",\"802154\",\"mac\",\"11\",\"1\",\"-60\",\"191\""}));

    model::BaselineWatch watch;
    assert(!watch.active());
    watch.captureFrom(scan, zig);
    assert(watch.active() && watch.apCount() == 2 && watch.panCount() == 1);

    // CSV round-trip preserves hostile SSIDs with commas/newlines and MFP flag.
    const std::string csv = watch.serializeCsv();
    model::BaselineWatch loaded;
    assert(loaded.parseCsv(csv));
    assert(loaded.apCount() == 2 && loaded.panCount() == 1);

    // Corrupt header rejected; corrupt lines skipped safely.
    model::BaselineWatch bad;
    assert(!bad.parseCsv("wrong-header\nAP,02:00:00:00:00:01,foo,WPA2,6,0\n"));
    assert(!bad.active());

    // Re-evaluating identical state raises ZERO anomalies.
    loaded.evaluate(scan, zig, 2000);
    assert(loaded.anomalies().empty() && loaded.highSeverityCount() == 0);

    // Now simulate live anomalies:
    // 1. Evil twin: familiar SSID "CorpNet,Lab\n1" from unfamiliar BSSID :99
    // 2. New AP: unfamiliar SSID + BSSID :98
    // 3. Auth downgrade: baseline BSSID :01 (WPA2) now seen as OPEN
    // 4. Hidden SSID on new BSSID :97 -> NewAp (NOT false EvilTwin!)
    // 5. New 802.15.4 PAN "beef"
    model::ScanModel live;
    live.begin();
    live.absorbPage(scanFrame({
        "\"1\",\"CorpNet,Lab\\x0a1\",\"02:00:00:00:00:01\",\"6\",\"OPEN\",\"-50\",\"" OCP_BAND_LABEL_24 "\"",
        "\"2\",\"CorpNet,Lab\\x0a1\",\"02:00:00:00:00:99\",\"6\",\"WPA2\",\"-48\",\"" OCP_BAND_LABEL_24 "\"",
        "\"3\",\"CoffeeGuest\",\"02:00:00:00:00:98\",\"1\",\"OPEN\",\"-70\",\"" OCP_BAND_LABEL_24 "\"",
        "\"4\",\"\",\"02:00:00:00:00:97\",\"11\",\"WPA2\",\"-65\",\"" OCP_BAND_LABEL_24 "\"",
    }), 3000);
    model::ZigModel live_zig;
    live_zig.begin();
    live_zig.absorb(zigFrame({
        "\"pan\",\"1a2b\",\"802154\",\"mac\",\"11\",\"1\",\"-60\",\"191\"",
        "\"pan\",\"beef\",\"802154\",\"mac\",\"15\",\"2\",\"-72\",\"140\"",
    }));

    loaded.evaluate(live, live_zig, 3000);
    assert(loaded.countOf(model::AnomalyKind::EvilTwin) == 1);
    assert(loaded.countOf(model::AnomalyKind::AuthDowngrade) == 1);
    assert(loaded.countOf(model::AnomalyKind::NewAp) == 2);
    assert(loaded.countOf(model::AnomalyKind::NewPan) == 1);
    assert(loaded.highSeverityCount() == 2);

    const auto *twin = loaded.anomalyForBssid("02:00:00:00:00:99");
    assert(twin && twin->kind == model::AnomalyKind::EvilTwin);
    const auto *down = loaded.anomalyForBssid("02:00:00:00:00:01");
    assert(down && down->kind == model::AnomalyKind::AuthDowngrade);
    const auto *hid = loaded.anomalyForBssid("02:00:00:00:00:97");
    assert(hid && hid->kind == model::AnomalyKind::NewAp);

    // MFP-dropped check via live [INSPECT] on baseline MFP-required AP :01.
    model::BaselineWatch mfp_watch;
    assert(mfp_watch.parseCsv(csv));
    model::Inspect ins;
    ins.valid = true;
    ins.bssid = "02:00:00:00:00:01";
    ins.beacons = 3;
    ins.mfp_required = false;
    mfp_watch.evaluateInspect(ins, 4000);
    assert(mfp_watch.countOf(model::AnomalyKind::MfpDropped) == 1);

    // High-severity anomaly evicts low-severity NewAp when table hits cap (64).
    model::BaselineWatch capped;
    assert(capped.parseCsv(csv));
    model::ScanModel flood;
    flood.begin();
    std::vector<std::string> flood_rows;
    for (int i = 0; i < 70; ++i) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "\"%d\",\"Net%d\",\"02:00:00:00:01:%02x\",\"1\",\"WPA2\",\"-70\",\"%s\"",
                      i + 1, i, i, OCP_BAND_LABEL_24);
        flood_rows.emplace_back(buf);
    }
    flood_rows.push_back("\"71\",\"CorpNet,Lab\\x0a1\",\"02:00:00:00:02:ff\",\"6\",\"OPEN\",\"-45\",\"" OCP_BAND_LABEL_24 "\"");
    flood.absorbPage(scanFrame(flood_rows), 5000);
    capped.evaluate(flood, zig, 5000);
    assert(capped.anomalies().size() == model::BaselineWatch::kMaxAnomalies);
    const auto *kept = capped.anomalyForBssid("02:00:00:00:02:ff");
    assert(kept && kept->kind == model::AnomalyKind::EvilTwin);

    // Precise scan baseline vs coarse beacon-event live rows: no false downgrades.
    model::ScanModel precise;
    precise.begin();
    precise.absorbPage(scanFrame({
        "\"1\",\"Wpa3Net\",\"02:00:00:00:03:01\",\"6\",\"WPA2/WPA3\",\"-50\",\"" OCP_BAND_LABEL_24 "\"",
        "\"2\",\"EapNet\",\"02:00:00:00:03:02\",\"6\",\"WPA2-EAP\",\"-50\",\"" OCP_BAND_LABEL_24 "\"",
        "\"3\",\"Wpa1Net\",\"02:00:00:00:03:03\",\"6\",\"WPA\",\"-50\",\"" OCP_BAND_LABEL_24 "\"",
        "\"4\",\"Wpa2Net\",\"02:00:00:00:03:04\",\"6\",\"WPA2\",\"-50\",\"" OCP_BAND_LABEL_24 "\"",
        "\"5\",\"OpenedNet\",\"02:00:00:00:03:05\",\"6\",\"WPA3\",\"-50\",\"" OCP_BAND_LABEL_24 "\"",
    }), 6000);
    model::ZigModel no_zig;
    model::BaselineWatch coarse_watch;
    coarse_watch.captureFrom(precise, no_zig);
    model::ScanModel events;
    events.beginContinuous();
    events.absorbEvent(netEvent("Wpa3Net", "02:00:00:00:03:01", 6, true, true, true, true), 6100);
    events.absorbEvent(netEvent("EapNet", "02:00:00:00:03:02", 6, true, true, false, false), 6100);
    events.absorbEvent(netEvent("Wpa1Net", "02:00:00:00:03:03", 6, true, false, false, false), 6100);
    events.absorbEvent(netEvent("Wpa2Net", "02:00:00:00:03:04", 6, true, false, false, false), 6100);
    events.absorbEvent(netEvent("OpenedNet", "02:00:00:00:03:05", 6, false, false, false, false), 6100);
    coarse_watch.evaluate(events, no_zig, 6100);
    assert(!coarse_watch.anomalyForBssid("02:00:00:00:03:01"));   // RSN can still be WPA3
    assert(!coarse_watch.anomalyForBssid("02:00:00:00:03:02"));   // RSN can still be EAP
    assert(!coarse_watch.anomalyForBssid("02:00:00:00:03:03"));   // no-RSN privacy can still be WPA1
    const auto *lost_rsn = coarse_watch.anomalyForBssid("02:00:00:00:03:04");
    assert(lost_rsn && lost_rsn->kind == model::AnomalyKind::AuthDowngrade);   // WPA2 lost its RSN IE
    const auto *opened = coarse_watch.anomalyForBssid("02:00:00:00:03:05");
    assert(opened && opened->kind == model::AnomalyKind::AuthDowngrade);       // WPA3 now open
    assert(coarse_watch.countOf(model::AnomalyKind::AuthDowngrade) == 2);

    // The baseline lives all session on a PSRAM-less deck: storage fits what it holds.
    {
        std::string big = "schema=oscilla-baseline-v1\n";
        model::ScanModel many;
        many.begin();
        ocp::Item page;
        page.tag = OCP_MARK_SCAN;
        page.kv = {{OCP_K_FIRST, "1"}, {OCP_K_TOTAL, "130"}};
        for (int i = 0; i < 130; ++i) {
            char mac[18], row[128];
            std::snprintf(mac, sizeof mac, "02:00:00:00:%02x:%02x", i >> 8, i & 0xff);
            big += std::string("AP,") + mac + ",net,WPA2,6,0\n";
            std::snprintf(row, sizeof row, "\"%d\",\"net\",\"%s\",\"6\",\"WPA2\",\"-50\",\"2.4\"", i + 1, mac);
            page.rows.push_back(row);
        }
        big += "AP,02:00:00:00:00:00,dup,WPA2,6,0\n";   // duplicate: reserved for, then trimmed
        model::BaselineWatch fitted;
        assert(fitted.parseCsv(big));
        assert(fitted.apCount() == 130 && fitted.aps().capacity() == 130);
        many.absorbPage(page);
        model::ZigModel none;
        fitted.captureFrom(many, none);
        assert(fitted.apCount() == 130 && fitted.aps().capacity() == 130);
        fitted.clear();
        assert(fitted.aps().capacity() == 0);
    }

    // Compact records: no per-AP strings; SSIDs share one pool; file format unchanged.
    {
        static_assert(sizeof(model::BaselineAp) == 12, "a baseline AP is 12 bytes");
        std::string csv = "schema=oscilla-baseline-v1\n";
        size_t ssid_bytes = 0;
        for (int i = 0; i < 130; ++i) {
            char line[96];
            std::snprintf(line, sizeof line, "AP,02:00:00:00:%02x:%02x,office-network-%09d,WPA2/WPA3,%d,%d\n",
                          i >> 8, i & 0xff, i, 1 + i % 11, i % 2);
            csv += line;
            ssid_bytes += 24;
        }
        csv += "AP,0a:00:00:00:00:01,caf\\xc3\\xa9\\x2c \\x22q\\x22,WPA3-EAP192,36,1\n";   // non-ASCII, comma, quotes
        csv += "AP,0a:00:00:00:00:02,,OPEN,1,0\n";                                           // hidden
        csv += "AP,0a:00:00:00:00:03,x,SOMETHING-NEW,1,0\n";                                  // unlisted auth
        csv += "PAN,0x1a2b,11\n";
        model::BaselineWatch w;
        assert(w.parseCsv(csv) && w.apCount() == 133 && w.panCount() == 1);
        const size_t heap = w.apHeapBytes();
        std::printf("  133-AP baseline (24-byte SSIDs) holds %zu bytes in two blocks\n", heap);
        std::fflush(stdout);
        assert(heap <= 133 * sizeof(model::BaselineAp) + ssid_bytes + 16 + 1);
        assert(w.ssidOf(w.aps()[130]) == std::string("caf\xc3\xa9, \"q\""));
        assert(w.ssidOf(w.aps()[131]).empty());

        std::string expect = csv;
        const std::string unlisted = "x,SOMETHING-NEW,";
        expect.replace(expect.find(unlisted), unlisted.size(), "x,UNKNOWN,");
        assert(w.serializeCsv() == expect);   // byte-exact round trip, unlisted auth named UNKNOWN

        // Lookups compare address bytes, so case does not matter; SSID matching stays exact.
        model::ScanModel live;
        live.begin();
        ocp::Item page;
        page.tag = OCP_MARK_SCAN;
        page.kv = {{OCP_K_FIRST, "1"}, {OCP_K_TOTAL, "3"}};
        page.rows = {"\"1\",\"office-network-000000005\",\"02:00:00:00:00:05\",\"6\",\"WPA2\",\"-50\",\"2.4\"",
                     "\"2\",\"office-network-000000007\",\"0A:00:00:00:00:09\",\"6\",\"WPA2/WPA3\",\"-50\",\"2.4\"",
                     "\"3\",\"office-network-00000000\",\"0A:00:00:00:00:01\",\"36\",\"OPEN\",\"-50\",\"5\""};
        live.absorbPage(page);
        model::ZigModel none;
        w.evaluate(live, none, 1);
        const auto *down = w.anomalyForBssid("02:00:00:00:00:05");
        assert(down && down->kind == model::AnomalyKind::AuthDowngrade && down->detail == "WPA2/WPA3->WPA2");
        const auto *twin = w.anomalyForBssid("0A:00:00:00:00:09");
        assert(twin && twin->kind == model::AnomalyKind::EvilTwin);
        const auto *upper = w.anomalyForBssid("0A:00:00:00:00:01");
        assert(upper && upper->kind == model::AnomalyKind::AuthDowngrade);   // matched despite upper case
        w.clear();
        assert(w.apHeapBytes() == 0);
    }

    std::cout << "baseline watch: capture, CSV round-trip, evil-twin, auth-downgrade (precise + coarse), MFP-drop, hidden-AP safety and eviction passed\n";
    return 0;
}
