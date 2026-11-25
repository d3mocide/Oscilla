/* Exercise the actual strip dispatcher across mid-frame view changes, and bound
 * what it allocates: the external panel must borrow the app's live tables, not
 * copy them (a copy of a full BLE/Wi-Fi table aborted the deck, WORKLOG
 * 2026-11-24). */
#ifndef OSCILLA_FRAME_DISPATCHER
#define OSCILLA_FRAME_DISPATCHER "ui/external_gps_display.cpp"
#endif
#include OSCILLA_FRAME_DISPATCHER
#include "storage/wardrive_logger.h"
#include <cstdlib>
#include <new>
#include <string>

namespace {
bool tracking = false;
size_t largest_alloc = 0;
long live_allocs = 0;
}

void *operator new(size_t n)
{
    if (tracking) { largest_alloc = std::max(largest_alloc, n); ++live_allocs; }
    if (void *p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept
{
    if (tracking && p) --live_allocs;
    std::free(p);
}
void operator delete(void *p, size_t) noexcept { operator delete(p); }

namespace ui {
void holdExternalTftReset() {}
bool beginExternalTft() { return true; }
bool setExternalTftSleep(bool) { return true; }
bool writeExternalTftSpan(int, int, int, const uint16_t *) { return true; }
}
namespace storage {
const WardriveLogStats &wardriveLogStats() { static WardriveLogStats stats; return stats; }
}

namespace {
std::string mac(unsigned i)
{
    char s[18];
    std::snprintf(s, sizeof s, "02:00:00:00:%02x:%02x", (i >> 8) & 0xff, i & 0xff);
    return s;
}

/* Tables at the size the combined run held when the deck aborted, or more. */
void fill(model::ScanModel &scan, model::BtModel &bt, model::ContactsModel &contacts)
{
    constexpr unsigned kAps = 300;
    scan.begin();
    ocp::Item ap;
    ap.tag = OCP_MARK_SCAN;
    ap.kv = {{OCP_K_FIRST, "1"}, {OCP_K_TOTAL, std::to_string(kAps)}};
    for (unsigned i = 1; i <= kAps; ++i)
        ap.rows.push_back("\"" + std::to_string(i) + "\",\"synthetic-network-" + std::to_string(i) + "\",\"" +
                          mac(i) + "\",\"6\",\"WPA2_PSK\",\"-" + std::to_string(40 + i % 50) + "\",\"2.4\"");
    scan.absorbPage(ap);
    ocp::Item ble;
    ble.tag = OCP_MARK_BLE;
    for (unsigned i = 0; i < model::BtModel::kMaxDevices; ++i)
        ble.rows.push_back("\"" + mac(0x1000 + i) + "\",\"synthetic-advertiser-" + std::to_string(i) +
                           "\",\"004c\",\"\",\"-" + std::to_string(40 + i % 50) + "\",\"1\"");
    bt.absorbScan(ble);
    ocp::Item client;
    client.tag = OCP_MARK_CLIENTS;
    for (unsigned i = 0; i < 200; ++i)
        client.rows.push_back("\"" + mac(1 + i % 20) + "\",\"" + mac(0x2000 + i) + "\",\"6\",\"2.4\",\"-54\",\"1\"");
    contacts.absorbClients(client);
}
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
    check(ui::frame_scan == &scan && ui::next_strip == 1,
          "Wi-Fi frame borrows the live model for its first strip");
    scan.clear();
    ui::tickExternalGpsDisplay(gps, 0, 0, 3001, &scan);
    check(ui::frame_scan == &scan && ui::next_strip == 2,
          "source mutation mid-frame is drawn from the live model, not a stale copy");
    ui::tickExternalGpsDisplay(gps, 0, 0, 3100, nullptr, 0, nullptr, 0, nullptr, 0, nullptr, &bt);
    check(ui::frame_scan == nullptr && ui::frame_bt == &bt,
          "mid-frame switch drops the Wi-Fi borrow");
    ui::tickExternalGpsDisplay(gps, 0, 0, 3200, nullptr, 0, nullptr, 0, nullptr, 0, nullptr,
                               nullptr, 0, &contacts);
    check(ui::frame_bt == nullptr && ui::frame_contacts == &contacts,
          "sniffer entry drops the BLE borrow");
    ui::tickExternalGpsDisplay(gps, 0, 0, 3300);
    check(ui::frame_contacts == nullptr, "leaving sniffer drops its borrow");
    check(bt.devices().size() == 1 && contacts.clients().size() == 1,
          "view changes preserve the authoritative cached inventories");

    /* Full tables, touring views the way deck_stability.py does: no display
     * allocation may need a table-sized contiguous block, and nothing is kept. */
    fill(scan, bt, contacts);
    check(scan.rows().size() == 300 && bt.devices().size() == model::BtModel::kMaxDevices &&
          contacts.clients().size() == 200, "full-size source tables are populated");
    uint32_t now = 10000;
    auto frames = [&](int which, int count) {
        for (int i = 0; i < count * 10; ++i, now += 5) {
            if (which == 0) ui::tickExternalGpsDisplay(gps, 0, 0, now, &scan, 7);
            else if (which == 1) ui::tickExternalGpsDisplay(gps, 0, 0, now, nullptr, 0, nullptr, 0, nullptr, 0,
                                                             nullptr, &bt, 40);
            else if (which == 2) ui::tickExternalGpsDisplay(gps, 0, 0, now, nullptr, 0, nullptr, 0, nullptr, 0,
                                                             nullptr, nullptr, 0, &contacts, false, 3);
            else ui::tickExternalGpsDisplay(gps, 0, 0, now);
        }
    };
    frames(3, 2);
    tracking = true;
    for (int tour = 0; tour < 4; ++tour)
        for (int view = 0; view < 4; ++view) frames(view, 3);
    tracking = false;
    std::printf("largest display allocation %zu bytes, %ld retained\n", largest_alloc, live_allocs);
    check(largest_alloc < 1024, "no display allocation needs a table-sized block");
    check(live_allocs == 0, "a view tour retains no display allocations");
    std::printf("external frame memory test: %d failures\n", failures);
    return failures ? 1 : 0;
}
