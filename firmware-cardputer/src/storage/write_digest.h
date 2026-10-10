/* Bounded log readback checksum; not a cryptographic authenticity check. */
#pragma once
#include <cstddef>
#include <cstdint>

namespace storage {
struct WriteDigest {
    uint32_t bytes = 0, hash = 2166136261u;
    void append(const uint8_t *data, size_t n)
    {
        bytes += static_cast<uint32_t>(n);
        while (n--) hash = (hash ^ *data++) * 16777619u;
    }
    bool matches(const WriteDigest &other) const { return bytes == other.bytes && hash == other.hash; }
};
}
