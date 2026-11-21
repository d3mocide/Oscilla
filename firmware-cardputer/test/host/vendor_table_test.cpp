/* Host tests for model/vendor_table.h (WP-12a). Also exposes --lookup-oui /
 * --lookup-bt CLI modes for tools/build_vendor_tables.py --selftest. */
#include "model/vendor_table.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

std::vector<uint8_t> readFile(const char *path)
{
    std::ifstream in(path, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
}

std::string lookupFromBuffer(const std::vector<uint8_t> &buf, const char magic[4],
                             uint16_t key_size, uint32_t target)
{
    if (buf.size() < model::kVendorHeaderSize) return {};
    uint32_t count = 0;
    if (!model::parseVendorHeader(buf.data(), magic, key_size, static_cast<uint32_t>(buf.size()), &count)) {
        return {};
    }
    return model::binarySearchVendor(count, target, key_size, [&](uint32_t idx, uint8_t out[model::kVendorRecordSize]) {
        const size_t off = model::kVendorHeaderSize + static_cast<size_t>(idx) * model::kVendorRecordSize;
        if (off + model::kVendorRecordSize > buf.size()) return false;
        std::memcpy(out, buf.data() + off, model::kVendorRecordSize);
        return true;
    });
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc == 4 && std::strcmp(argv[1], "--lookup-oui") == 0) {
        uint32_t key = 0;
        if (model::parseOuiKey(argv[3], &key) && !model::isLocallyAdministeredOui(key)) {
            auto buf = readFile(argv[2]);
            std::puts(lookupFromBuffer(buf, "OUI1", model::kOuiKeySize, key).c_str());
        } else {
            std::puts("");
        }
        return 0;
    }
    if (argc == 4 && std::strcmp(argv[1], "--lookup-bt") == 0) {
        uint16_t key = 0;
        if (model::parseBtCompanyId(argv[3], &key)) {
            auto buf = readFile(argv[2]);
            std::puts(lookupFromBuffer(buf, "BTM1", model::kBtMfrKeySize, key).c_str());
        } else {
            std::puts("");
        }
        return 0;
    }

    // OUI parsing & locally-administered detection.
    uint32_t oui = 0;
    assert(model::parseOuiKey("FC:F5:C4:11:22:33", &oui) && oui == 0xFCF5C4U);
    assert(!model::isLocallyAdministeredOui(oui));
    assert(model::parseOuiKey("02:00:00:00:00:01", &oui) && oui == 0x020000U);
    assert(model::isLocallyAdministeredOui(oui));
    assert(!model::parseOuiKey("ZZ:00:00", &oui));
    assert(!model::parseOuiKey("00:11", &oui));

    // BT Company ID parsing.
    uint16_t cid = 0;
    assert(model::parseBtCompanyId("004c", &cid) && cid == 0x004CU);
    assert(model::parseBtCompanyId("0x00E0", &cid) && cid == 0x00E0U);
    assert(!model::parseBtCompanyId("10000", &cid));
    assert(!model::parseBtCompanyId("0xZZ", &cid));
    assert(!model::parseBtCompanyId("", &cid));

    // Header validation rejects bad magic, bad record/key size, and truncated files.
    uint8_t hdr[16] = {'O','U','I','1', 2,0,0,0, 24,0, 3,0, 0,0,0,0};
    uint32_t cnt = 0;
    assert(model::parseVendorHeader(hdr, "OUI1", 3, 16 + 48, &cnt) && cnt == 2);
    assert(!model::parseVendorHeader(hdr, "BTM1", 3, 16 + 48, &cnt));
    assert(!model::parseVendorHeader(hdr, "OUI1", 2, 16 + 48, &cnt));
    assert(!model::parseVendorHeader(hdr, "OUI1", 3, 16 + 47, &cnt));  // truncated

    // Sanitization strips control/non-ASCII bytes.
    const uint8_t hostile[] = {'A','c','m','e', 0x07, 0x1B, 0xFF, ' ', ' ', 0};
    assert(model::sanitizeVendorName(hostile, sizeof hostile) == "Acme");

    // LRU cache caches both hits and misses and evicts oldest entry.
    model::VendorCache<uint16_t, 2> cache;
    std::string out;
    assert(!cache.find(1, &out));
    cache.put(1, "Apple");
    cache.put(2, "");  // negative cache (miss)
    assert(cache.find(1, &out) && out == "Apple");  // touch 1 so 2 is LRU
    assert(cache.find(2, &out) && out.empty());     // touch 2 so 1 is LRU
    cache.put(3, "Google");                         // evicts 1
    assert(!cache.find(1, &out));
    assert(cache.find(2, &out) && out.empty());
    assert(cache.find(3, &out) && out == "Google");

    std::cout << "vendor table: OUI/BT key parsing, header bounds, sanitization and LRU cache passed\n";
    return 0;
}
