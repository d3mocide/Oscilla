/* Exercise the actual strip dispatcher across mid-frame view changes. */
#ifndef OSCILLA_FRAME_DISPATCHER
#define OSCILLA_FRAME_DISPATCHER "ui/external_gps_display.cpp"
#endif
#include OSCILLA_FRAME_DISPATCHER
#include "storage/wardrive_logger.h"

namespace ui {
void holdExternalTftReset() {}
bool beginExternalTft() { return true; }
bool writeExternalTftSpan(int, int, int, const uint16_t *) { return true; }
}
namespace storage {
const WardriveLogStats &wardriveLogStats() { static WardriveLogStats stats; return stats; }
}

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char *label) {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
        if (!ok) ++failures;
    };
    model::GnssModel gps;
    model::ScanModel scan;
    scan.begin();
    ocp::Item ap;
    ap.tag = OCP_MARK_SCAN;
    ap.kv = {{OCP_K_FIRST, "1"}, {OCP_K_TOTAL, "1"}};
    ap.rows = {"\"1\",\"synthetic\",\"02:00:00:00:00:01\",\"6\",\"OPEN\",\"-54\",\"2.4\""};
    scan.absorbPage(ap);
    model::BtModel bt;
    ocp::Item beacon;
    beacon.tag = OCP_MARK_BLE;
    beacon.rows = {"\"02:00:00:00:00:02\",\"synthetic\",\"004c\",\"\",\"-54\",\"1\""};
    bt.absorbScan(beacon);
    model::ContactsModel contacts;
    ocp::Item client;
    client.tag = OCP_MARK_CLIENTS;
    client.rows = {"\"02:00:00:00:00:01\",\"02:00:00:00:00:03\",\"6\",\"2.4\",\"-54\",\"1\""};
    contacts.absorbClients(client);
    check(scan.rows().size() == 1 && bt.devices().size() == 1 && contacts.clients().size() == 1,
          "synthetic source inventories are populated");
    ui::enabled = true;
    ui::tickExternalGpsDisplay(gps, 0, 0, 3000, &scan);
    check(ui::frame_scan.rows().size() == 1 && ui::next_strip == 1,
          "Wi-Fi frame freezes a complete model for its first strip");
    scan.clear();
    ui::tickExternalGpsDisplay(gps, 0, 0, 3001, &scan);
    check(ui::frame_scan.rows().size() == 1 && ui::next_strip == 2,
          "source mutation cannot tear the in-progress frame");
    ui::tickExternalGpsDisplay(gps, 0, 0, 3100, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, &bt);
    check(ui::frame_scan.rows().empty() && ui::frame_bt.devices().size() == 1,
          "mid-frame switch frees the old Wi-Fi display copy");
    ui::tickExternalGpsDisplay(gps, 0, 0, 3200, nullptr, 0, nullptr, 0, nullptr, 0, nullptr,
                               nullptr, 0, &contacts);
    check(ui::frame_bt.devices().capacity() == 0 && ui::frame_contacts.clients().size() == 1,
          "sniffer entry frees the old BLE display copy");
    ui::tickExternalGpsDisplay(gps, 0, 0, 3300);
    check(ui::frame_contacts.clients().capacity() == 0,
          "leaving sniffer frees its display copy");
    check(bt.devices().size() == 1 && contacts.clients().size() == 1,
          "view changes preserve the authoritative cached inventories");
    std::printf("external frame memory test: %d failures\n", failures);
    return failures ? 1 : 0;
}
