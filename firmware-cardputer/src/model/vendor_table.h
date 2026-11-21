/*
 * vendor_table.h — pure-C++ sorted binary vendor lookup (OUI & BT Company ID)
 * and fixed-size RAM cache (WP-12a). Zero SD/Arduino dependency; host-tested in
 * test/host/vendor_table_test.cpp and cross-checked with tools/build_vendor_tables.py.
 *
 * Binary file layout (little-endian header + sorted fixed 24-byte records):
 *   Header (16 B): magic[4] ("OUI1" or "BTM1"), uint32 count,
 *                  uint16 record_size (24), uint16 key_size (3 or 2), uint32 reserved (0)
 *   OUI record (24 B): uint8 key[3] (big-endian MAC prefix), char name[21]
 *   BTM record (24 B): uint8 key[2] (little-endian uint16 company ID), char name[22]
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace model {

constexpr size_t kVendorHeaderSize = 16;
constexpr size_t kVendorRecordSize = 24;
constexpr uint16_t kOuiKeySize = 3;
constexpr uint16_t kBtMfrKeySize = 2;

inline int vendorHexVal(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

/* Parses first 3 octets of "AA:BB:CC:..." or "AABBCC..." into 24-bit key. */
inline bool parseOuiKey(const std::string &mac, uint32_t *out)
{
    if (!out) return false;
    uint8_t octets[3] = {};
    size_t pos = 0;
    for (int i = 0; i < 3; ++i) {
        if (pos + 2 > mac.size()) return false;
        const int hi = vendorHexVal(mac[pos]);
        const int lo = vendorHexVal(mac[pos + 1]);
        if (hi < 0 || lo < 0) return false;
        octets[i] = static_cast<uint8_t>((hi << 4) | lo);
        pos += 2;
        if (i < 2 && pos < mac.size() && (mac[pos] == ':' || mac[pos] == '-')) ++pos;
    }
    *out = (static_cast<uint32_t>(octets[0]) << 16) |
           (static_cast<uint32_t>(octets[1]) << 8) |
           static_cast<uint32_t>(octets[2]);
    return true;
}

/* Locally-administered unicast bit (bit 1 of first octet, bit 0 clear). */
inline bool isLocallyAdministeredOui(uint32_t oui_key)
{
    const uint8_t b0 = static_cast<uint8_t>((oui_key >> 16) & 0xFF);
    return (b0 & 0x02U) != 0 && (b0 & 0x01U) == 0;
}

/* Parses "004c" or "0x004C" into a 16-bit Bluetooth SIG Company ID. */
inline bool parseBtCompanyId(const std::string &hex, uint16_t *out)
{
    if (!out || hex.empty()) return false;
    size_t start = 0;
    if (hex.size() >= 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) start = 2;
    const size_t digits = hex.size() - start;
    if (digits == 0 || digits > 4) return false;
    uint32_t v = 0;
    for (size_t i = start; i < hex.size(); ++i) {
        const int n = vendorHexVal(hex[i]);
        if (n < 0) return false;
        v = (v << 4) | static_cast<uint32_t>(n);
    }
    *out = static_cast<uint16_t>(v);
    return true;
}

inline uint32_t vendorReadLe32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline uint16_t vendorReadLe16(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0]) | static_cast<uint16_t>(static_cast<uint16_t>(p[1]) << 8);
}

inline bool parseVendorHeader(const uint8_t hdr[kVendorHeaderSize], const char magic[4],
                              uint16_t expected_key_size, uint32_t file_size, uint32_t *out_count)
{
    if (!hdr || !magic || !out_count) return false;
    if (std::memcmp(hdr, magic, 4) != 0) return false;
    const uint32_t count = vendorReadLe32(hdr + 4);
    const uint16_t rec_size = vendorReadLe16(hdr + 8);
    const uint16_t key_size = vendorReadLe16(hdr + 10);
    if (rec_size != kVendorRecordSize || key_size != expected_key_size) return false;
    if (count > 200000U) return false;
    const uint64_t needed = static_cast<uint64_t>(kVendorHeaderSize) +
                            static_cast<uint64_t>(count) * kVendorRecordSize;
    if (needed > file_size) return false;
    *out_count = count;
    return true;
}

inline std::string sanitizeVendorName(const uint8_t *bytes, size_t max_len)
{
    std::string out;
    out.reserve(max_len);
    for (size_t i = 0; i < max_len; ++i) {
        if (bytes[i] == 0) break;
        if (bytes[i] >= 0x20 && bytes[i] <= 0x7E) out.push_back(static_cast<char>(bytes[i]));
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

inline uint32_t extractVendorRecordKey(const uint8_t rec[kVendorRecordSize], uint16_t key_size)
{
    if (key_size == kOuiKeySize) {
        return (static_cast<uint32_t>(rec[0]) << 16) |
               (static_cast<uint32_t>(rec[1]) << 8) |
               static_cast<uint32_t>(rec[2]);
    }
    return vendorReadLe16(rec);
}

/* Bounded O(log n) binary search over fixed 24-byte sorted records.
 * `read_record(uint32_t index, uint8_t out[24])` returns true on full read. */
template <class ReadRecordFn>
inline std::string binarySearchVendor(uint32_t count, uint32_t target_key,
                                      uint16_t key_size, ReadRecordFn read_record)
{
    if (!count || (key_size != kOuiKeySize && key_size != kBtMfrKeySize)) return {};
    uint32_t lo = 0, hi = count;
    uint8_t rec[kVendorRecordSize];
    for (unsigned step = 0; step < 24 && lo < hi; ++step) {
        const uint32_t mid = lo + (hi - lo) / 2;
        if (!read_record(mid, rec)) return {};
        const uint32_t key = extractVendorRecordKey(rec, key_size);
        if (key == target_key) {
            return sanitizeVendorName(rec + key_size, kVendorRecordSize - key_size);
        }
        if (key < target_key) lo = mid + 1;
        else hi = mid;
    }
    return {};
}

/* Fixed-size RAM cache for both hits and misses (zero dynamic growth). */
template <typename KeyT, size_t Capacity>
class VendorCache {
    struct Slot {
        bool used = false;
        KeyT key{};
        std::string value;
        uint32_t age = 0;
    };
    std::array<Slot, Capacity> slots_{};
    uint32_t clock_ = 0;

public:
    void clear()
    {
        for (auto &s : slots_) s = {};
        clock_ = 0;
    }

    bool find(KeyT key, std::string *out)
    {
        for (auto &s : slots_) {
            if (s.used && s.key == key) {
                s.age = ++clock_;
                if (out) *out = s.value;
                return true;
            }
        }
        return false;
    }

    void put(KeyT key, const std::string &value)
    {
        size_t victim = 0;
        for (size_t i = 0; i < Capacity; ++i) {
            if (!slots_[i].used || slots_[i].key == key) { victim = i; break; }
            if (slots_[i].age < slots_[victim].age) victim = i;
        }
        slots_[victim] = {true, key, value, ++clock_};
    }
};

}  // namespace model
