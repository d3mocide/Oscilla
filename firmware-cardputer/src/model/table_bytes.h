/* Rough heap held by a model table, for the debug dump only: element storage
 * (vector capacity, deque size) plus every string too long for libstdc++'s
 * 15-byte inline buffer. Allocator headers and deque maps are not counted. */
#pragma once
#include <cstddef>
#include <string>

namespace model {
inline size_t stringHeap(const std::string &s) { return s.capacity() > 15 ? s.capacity() + 1 : 0; }

template<class... S>
size_t stringsHeap(const S &...s) { return (stringHeap(s) + ... + 0); }

template<class Table, class Strings>
size_t tableBytes(const Table &t, size_t slots, Strings strings)
{
    size_t bytes = slots * sizeof(typename Table::value_type);
    for (const auto &e : t) bytes += strings(e);
    return bytes;
}
}  // namespace model
