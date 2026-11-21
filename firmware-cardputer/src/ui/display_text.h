/* Safe deck labels; control bytes never reach the display. */
#pragma once
#include <string>
namespace ui {
inline std::string printable(const std::string &s, size_t max_len)
{
    std::string out;
    for (unsigned char c : s) {
        if (out.size() >= max_len) break;
        out += (c >= 0x20 && c <= 0x7E) ? static_cast<char>(c) : '?';
    }
    return out;
}

}
