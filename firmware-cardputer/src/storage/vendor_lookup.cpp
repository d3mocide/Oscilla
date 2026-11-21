/*
 * vendor_lookup.cpp — see vendor_lookup.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/vendor_lookup.h"

#include <SD.h>

#include "model/vendor_table.h"
#include "storage/sd_storage.h"

namespace storage {

namespace {

constexpr const char *kOuiPath = "/oscilla/oui.bin";
constexpr const char *kBtMfrPath = "/oscilla/bt_mfr.bin";
constexpr uint32_t kVendorLockMs = 2;

struct TableState {
    bool probed = false;
    bool present = false;
    uint32_t count = 0;
};

TableState g_oui_state, g_bt_state;
model::VendorCache<uint32_t, 32> g_oui_cache;
model::VendorCache<uint16_t, 32> g_bt_cache;

bool probeLocked(const char *path, const char magic[4], uint16_t key_size, TableState &st)
{
    if (st.probed) return st.present;
    st.probed = true;
    st.present = false;
    st.count = 0;
    File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory()) {
        if (f) f.close();
        return false;
    }
    uint8_t hdr[model::kVendorHeaderSize];
    const uint32_t fsize = static_cast<uint32_t>(f.size());
    const int got = fsize >= model::kVendorHeaderSize ? f.read(hdr, sizeof hdr) : 0;
    f.close();
    uint32_t count = 0;
    if (got == static_cast<int>(sizeof hdr) &&
        model::parseVendorHeader(hdr, magic, key_size, fsize, &count)) {
        st.present = count > 0;
        st.count = count;
    }
    return st.present;
}

std::string searchTable(const char *path, const char magic[4], uint16_t key_size,
                        TableState &st, uint32_t target_key, bool *completed)
{
    *completed = false;
    if (!ready()) return {};
    if (st.probed && !st.present) {
        *completed = true;
        return {};
    }
    if (!lock(kVendorLockMs)) return {};
    if (!probeLocked(path, magic, key_size, st)) {
        unlock();
        *completed = true;
        return {};
    }
    File f = SD.open(path, FILE_READ);
    std::string found;
    if (f && !f.isDirectory()) {
        found = model::binarySearchVendor(
            st.count, target_key, key_size,
            [&](uint32_t index, uint8_t out[model::kVendorRecordSize]) -> bool {
                const uint32_t offset = static_cast<uint32_t>(model::kVendorHeaderSize) +
                                        index * static_cast<uint32_t>(model::kVendorRecordSize);
                return f.seek(offset) &&
                       f.read(out, model::kVendorRecordSize) == static_cast<int>(model::kVendorRecordSize);
            });
        *completed = true;
    }
    if (f) f.close();
    unlock();
    return found;
}

}  // namespace

std::string lookupOuiVendor(const std::string &mac)
{
    uint32_t key = 0;
    if (!model::parseOuiKey(mac, &key)) return {};
    if (model::isLocallyAdministeredOui(key)) return {};
    std::string cached;
    if (g_oui_cache.find(key, &cached)) return cached;
    bool completed = false;
    std::string found = searchTable(kOuiPath, "OUI1", model::kOuiKeySize, g_oui_state, key, &completed);
    if (completed) g_oui_cache.put(key, found);
    return found;
}

std::string lookupBtCompany(const std::string &mfr_hex)
{
    uint16_t key = 0;
    if (!model::parseBtCompanyId(mfr_hex, &key)) return {};
    std::string cached;
    if (g_bt_cache.find(key, &cached)) return cached;
    bool completed = false;
    std::string found = searchTable(kBtMfrPath, "BTM1", model::kBtMfrKeySize, g_bt_state, key, &completed);
    if (completed) g_bt_cache.put(key, found);
    return found;
}

void resetVendorLookup()
{
    g_oui_state = {};
    g_bt_state = {};
    g_oui_cache.clear();
    g_bt_cache.clear();
}

bool ouiTablePresent() { return g_oui_state.probed && g_oui_state.present; }
bool btMfrTablePresent() { return g_bt_state.probed && g_bt_state.present; }

}  // namespace storage
