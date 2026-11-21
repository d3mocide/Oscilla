/* Snapshot refresh must work without a second table-sized allocation. */
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include "ocp.h"
#include "model/contacts_model.h"
#include "model/bt_model.h"

namespace {
size_t allocation_limit = size_t(-1);
int failures = 0;
void check(bool ok, const char *label) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}
ocp::Item snapshot(const char *tag, size_t count) {
    ocp::Item frame;
    frame.tag = tag;
    for (size_t i = 0; i < count; ++i) {
        char mac[18];
        std::snprintf(mac, sizeof mac, "02:00:00:00:%02x:%02x",
                      unsigned(i >> 8), unsigned(i & 255));
        std::string row = std::string("\"") + mac + "\",";
        if (frame.tag == OCP_MARK_CLIENTS)
            row += "\"02:00:00:00:ff:ff\",\"6\",\"2.4\",\"-54\",\"42\"";
        else if (frame.tag == OCP_MARK_PROBES)
            row += "\"synthetic-ssid\",\"-54\",\"42\"";
        else
            row += "\"synthetic-name\",\"004c\",\"\",\"-54\",\"42\"";
        frame.rows.push_back(std::move(row));
    }
    return frame;
}
template<class Refresh>
void constrained(Refresh refresh, size_t limit, const char *label) {
    bool ok = true;
    allocation_limit = limit;
    try { refresh(); } catch (const std::bad_alloc &) { ok = false; }
    allocation_limit = size_t(-1);
    check(ok, label);
}
}

void *operator new(size_t n) {
    if (n > allocation_limit) throw std::bad_alloc();
    if (void *p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }

int main() {
    model::ContactsModel contacts;
    auto clients = snapshot(OCP_MARK_CLIENTS, 96);
    auto probes = snapshot(OCP_MARK_PROBES, 96);
    contacts.absorbClients(clients, 1);
    contacts.absorbProbes(probes, 1);
    constrained([&] { contacts.absorbClients(clients, 2); }, 1024,
                "client refresh keeps the existing table allocation");
    constrained([&] { contacts.absorbProbes(probes, 2); }, 1024,
                "request refresh keeps the existing table allocation");
    check(contacts.clients().size() == 96 && contacts.probes().size() == 96 &&
          contacts.clientSnapshotMs() == 2 && contacts.probeSnapshotMs() == 2,
          "constrained refresh commits both complete snapshots");

    model::ContactsModel growing;
    auto small = snapshot(OCP_MARK_CLIENTS, 63);
    auto larger = snapshot(OCP_MARK_CLIENTS, 65);
    growing.absorbClients(small);
    constrained([&] { growing.absorbClients(larger); }, 65*sizeof(model::ClientRow),
                "63-to-65 client growth needs only the final table size");
    check(growing.clients().size() == 65, "growth keeps every valid row");
    larger.rows.resize(3);
    larger.rows[1] = "malformed";
    growing.absorbClients(larger, 3);
    check(growing.clients().size() == 2 && growing.malformedRows() == 1 &&
          growing.clients()[1].bssid == "02:00:00:00:00:02",
          "smaller refresh removes stale rows and skips malformed input");

    model::BtModel bt;
    auto devices = snapshot(OCP_MARK_BLE, 96);
    bt.absorbScan(devices, 1);
    constrained([&] { bt.absorbScan(devices, 2); }, 1024,
                "advertiser refresh keeps the existing table allocation");
    check(bt.devices().size() == 96 && bt.devices()[95].observed_ms == 2,
          "constrained advertiser refresh updates the full table");
    contacts.stopSniffing(); bt.stop();
    check(contacts.clients().size() == 96 && bt.devices().size() == 96,
          "stop retains cached contact and advertiser inventories");
    std::printf("snapshot memory test: %d failures\n", failures);
    return failures ? 1 : 0;
}
