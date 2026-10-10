/* Bounded raw-byte display rows; decoded controls never reach a renderer. */
#pragma once
#include <string>
#include <cstddef>
namespace model {
struct PayloadLine { std::string hex, ascii; size_t bytes = 0; };
inline int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c-'0';
    if (c >= 'a' && c <= 'f') return c-'a'+10;
    if (c >= 'A' && c <= 'F') return c-'A'+10;
    return -1;
}
inline PayloadLine payloadLine(const std::string &hex, size_t offset) {
    PayloadLine out;
    if (hex.size()%2 || offset >= hex.size()/2) return out;
    const char *digits = "0123456789ABCDEF";
    for (size_t i = offset; i < hex.size()/2 && out.bytes < 8; ++i) {
        const int hi = hexNibble(hex[i*2]), lo = hexNibble(hex[i*2+1]);
        if (hi < 0 || lo < 0) return {};
        if (out.bytes) out.hex += ' ';
        out.hex += digits[hi]; out.hex += digits[lo];
        const int byte = hi*16+lo;
        out.ascii += byte >= 32 && byte <= 126 ? static_cast<char>(byte) : '.';
        ++out.bytes;
    }
    return out;
}
}
