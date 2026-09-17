/* number_parse.h — whole-token numeric parsing for hostile OCP fields. */
#pragma once

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace model {

inline bool parseUnsigned(const std::string &text, uint64_t *out)
{
    if (text.empty() || !out || text[0] == '+' || text[0] == '-' ||
        std::isspace(static_cast<unsigned char>(text[0]))) return false;
    char *end = nullptr;
    errno = 0;
    unsigned long long value = std::strtoull(text.c_str(), &end, 10);
    if (errno || end != text.c_str() + text.size()) return false;
    *out = static_cast<uint64_t>(value);
    return true;
}

inline bool parseSigned(const std::string &text, int64_t *out)
{
    if (text.empty() || !out || text[0] == '+' ||
        std::isspace(static_cast<unsigned char>(text[0]))) return false;
    char *end = nullptr;
    errno = 0;
    long long value = std::strtoll(text.c_str(), &end, 10);
    if (errno || end != text.c_str() + text.size()) return false;
    *out = static_cast<int64_t>(value);
    return true;
}

inline bool parseFiniteDouble(const std::string &text, double *out)
{
    if (text.empty() || !out || std::isspace(static_cast<unsigned char>(text[0]))) return false;
    char *end = nullptr;
    errno = 0;
    double value = std::strtod(text.c_str(), &end);
    if (errno || end != text.c_str() + text.size() || !std::isfinite(value)) return false;
    *out = value;
    return true;
}

}  // namespace model
