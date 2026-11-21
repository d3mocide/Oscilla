#include "storage/session_reader.h"
#include <SD.h>
#include <iostream>
uint32_t session_test_now = 0;
bool session_test_locked = false;
bool deny_lock = false, mounted = true;
size_t session_test_read_bytes = 0, session_test_entries = 0;
SessionTestSD SD;
namespace storage {
bool ready() {
    return mounted;
}
bool lock(uint32_t timeout) {
    assert(timeout == 0 && !session_test_locked);
    if (deny_lock) return false;
    session_test_locked = true;
    return true;
}
void unlock() {
    assert(session_test_locked);
    session_test_locked = false;
}
} // namespace storage
static auto node(const std::string &name, const std::string &data = "", bool dir = false) {
    auto n = std::make_shared<SessionTestNode>();
    n->name = name;
    n->data = data;
    n->directory = dir;
    return n;
}
static void tick(model::SessionReview &r) {
    session_test_now += 10;
    session_test_read_bytes = session_test_entries = 0;
    storage::sessionTick(r, session_test_now);
    assert(session_test_read_bytes <= 512 && session_test_entries <= 1 && !session_test_locked);
}
static void drain(model::SessionReview &r) {
    unsigned i = 0;
    while (r.busy && i++ < 2000) tick(r);
    assert(!r.busy);
}
int main() {
    auto dir = node("/oscilla/lora", "", true);
    SD.nodes[dir->name] = dir;
    auto csv = node("log_0001.csv", "ts_ms,freq_hz,sf,bw_khz,cr,rssi,snr,len,hex,framing\n");
    for (int i = 0; i < 50; ++i)
        csv->data += std::to_string(1000 + i) + ",910525000,7,62,1,-61,4.5,1,41,unknown\n";
    dir->children = {node("log_0001.meta", "profile=test\n"), csv, node("ignored.csv"),
                     node("log_0001.health.csv")};
    SD.nodes["/oscilla/lora/log_0001.csv"] = csv;
    model::SessionReview r;
    storage::sessionCatalog(r);
    drain(r);
    assert(r.catalog.entries.size() == 1 &&
           r.catalog.entries[0].parts == (model::Csv | model::Manifest | model::Health));
    storage::sessionOpen(r);
    tick(r);
    const auto original = csv->data.size();
    csv->data += "9000,910525000,7,62,1,-70,1.5,1,42,unknown\n";
    drain(r);
    assert(r.snapshot_bytes == original && r.summary.valid == 50 && r.rows.size() == 6);
    r.stage = model::ReviewStage::Records;
    storage::sessionPage(r, 1);
    drain(r);
    assert(r.summary.valid == 50 && r.rows.size() == 6 && r.rows[0].packet.received_ms == 1006);
    assert(r.read_bytes < r.snapshot_bytes);
    storage::sessionPage(r, 2);
    deny_lock = true;
    tick(r);
    assert(r.busy && session_test_read_bytes == 0);
    deny_lock = false;
    drain(r);
    assert(r.rows[0].packet.received_ms == 1012);
    csv->data.resize(10);
    storage::sessionPage(r, 0);
    drain(r);
    assert(!r.error.empty());
    tick(r);
    storage::sessionCatalog(r);
    tick(r);
    storage::sessionCancel(r);
    tick(r);
    assert(!r.busy&&r.limited);
    mounted = false;
    storage::sessionCatalog(r);
    drain(r);
    assert(!r.available && !r.error.empty());
    tick(r);
    mounted = true;
    storage::sessionCatalog(r);
    deny_lock = true;
    session_test_now += 6000;
    tick(r);
    assert(!r.busy && !r.error.empty());
    deny_lock = false;
    tick(r);
    std::cout << "session reader: read-only locks, 512-byte/one-entry budget, fixed snapshots, paging, "
                 "contention, shrink, cancellation and missing SD passed\n";
}
