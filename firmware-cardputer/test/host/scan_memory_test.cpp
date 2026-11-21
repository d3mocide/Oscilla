/* AP growth and frozen display copies must tolerate fragmented free RAM. */
#include <cstdio>
#include <cstdlib>
#include <new>
#include "ocp.h"
#include "model/scan_model.h"

namespace {
size_t allocation_limit = size_t(-1), largest_request = 0;
int failures = 0;
void check(bool ok, const char *label) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}
ocp::Item event(size_t i) {
    char mac[18];
    std::snprintf(mac, sizeof mac, "02:00:00:00:%02x:%02x", unsigned(i>>8), unsigned(i&255));
    ocp::Item e;
    e.kind = ocp::ItemKind::Event;
    e.kv = {{OCP_K_KIND,OCP_EVT_KIND_NETWORK}, {OCP_K_BSSID,mac},
            {OCP_K_SSID,std::string(32,'S')}, {OCP_K_BAND,OCP_BAND_LABEL_24},
            {OCP_K_CH,"6"}, {OCP_K_RSSI,"-54"}, {OCP_K_PRIVACY,"1"},
            {OCP_K_RSN,"1"}, {OCP_K_MFP_CAPABLE,"1"}, {OCP_K_MFP_REQUIRED,"0"},
            {OCP_K_INTERVAL_MS,"100"}};
    return e;
}
}
void *operator new(size_t n) {
    if (n > largest_request) largest_request = n;
    if (n > allocation_limit) throw std::bad_alloc();
    if (void *p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }

int main() {
    model::ScanModel scan, frozen;
    bool ok = true;
    largest_request = 0;
    allocation_limit = 4096;
    try {
        for (unsigned cycle = 0; cycle < 4; ++cycle) {
            scan.beginContinuous();
            for (size_t i = 0; i <= model::ScanModel::kMaxRows; ++i) {
                scan.absorbEvent(event(i), 100+i);
                if (i == 63 || i == 127 || i == 255) frozen = scan;
            }
            frozen = scan;
            check(scan.rows().size() == model::ScanModel::kMaxRows &&
                  frozen.rows().size() == scan.rows().size(),
                  "full live inventory and frozen copy keep the existing row cap");
            auto update = event(0);
            update.kv[5].value = "-42";
            scan.absorbEvent(update, 9999);
            check(scan.rows()[0].rssi == -42 && frozen.rows()[0].rssi == -54,
                  "full-table upsert preserves frozen display data");
            scan.stop();
            check(scan.rows().size() == model::ScanModel::kMaxRows && !scan.continuousActive(),
                  "stop keeps every cached AP");
            scan.clear(); frozen.clear();
            check(scan.rows().empty() && frozen.rows().empty(), "restart releases old AP rows");
        }
        scan.begin();
        for (size_t start = 0; start < model::ScanModel::kMaxRows; start += 64) {
            ocp::Item page;
            page.tag = OCP_MARK_SCAN;
            page.kv = {{OCP_K_FIRST,std::to_string(start+1)},
                       {OCP_K_TOTAL,std::to_string(model::ScanModel::kMaxRows)}};
            for (size_t i = start; i < start+64; ++i) {
                char row[160];
                std::snprintf(row, sizeof row,
                    "\"%zu\",\"synthetic\",\"02:00:00:00:%02x:%02x\",\"6\",\"OPEN\",\"-54\",\"2.4\"",
                    i+1, unsigned(i>>8), unsigned(i&255));
                page.rows.emplace_back(row);
            }
            const auto next = scan.absorbPage(page, 1000+start);
            check(next == (start+64 == model::ScanModel::kMaxRows ? 0 : start+65),
                  "paged scan preserves continuation and final-page semantics");
        }
        check(scan.rows().size() == model::ScanModel::kMaxRows && !scan.scanning() &&
              scan.rows().back().idx == model::ScanModel::kMaxRows,
              "paged inventory retains every row under the allocation ceiling");
    } catch (const std::bad_alloc &) { ok = false; }
    allocation_limit = size_t(-1);
    check(ok, "live growth and display resampling need no block larger than 4 KiB");
    std::printf("scan memory test: %d failures, largest allocation %zu bytes\n", failures, largest_request);
    return failures ? 1 : 0;
}
