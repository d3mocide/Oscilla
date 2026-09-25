/*
 * meter_cmd.cpp — see meter_cmd.h.
 *
 * SPDX-License-Identifier: MIT
 */
#include "meter_cmd.h"

#include <cstring>

namespace meter {
namespace {

/* Whole-token decimal, overflow-checked. */
bool parseU32(const char *s, uint32_t lo, uint32_t hi, uint32_t &out)
{
    if (!s || !*s) return false;
    uint64_t v = 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return false;
        v = v * 10 + static_cast<uint64_t>(*s - '0');
        if (v > hi) return false;
    }
    if (v < lo) return false;
    out = static_cast<uint32_t>(v);
    return true;
}

bool labelChar(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == '_' || c == '.' || c == '-';
}

Cmd invalid(const char *why)
{
    Cmd c;
    c.err = why;
    return c;
}

}  // namespace

Cmd parseCommand(const char *line, bool overflow)
{
    if (overflow) return invalid("too long");

    char buf[97];
    size_t len = std::strlen(line);
    if (len >= sizeof buf) return invalid("too long");
    std::memcpy(buf, line, len + 1);

    char *verb = buf;
    char *arg = std::strchr(buf, ' ');
    if (arg) {
        *arg++ = '\0';
        if (std::strchr(arg, ' ')) return invalid("one argument at most");
    }

    Cmd c;
    if (std::strcmp(verb, "info") == 0 && !arg) {
        c.kind = CmdKind::Info;
    } else if (std::strcmp(verb, "stream") == 0 && arg && std::strcmp(arg, "on") == 0) {
        c.kind = CmdKind::StreamOn;
    } else if (std::strcmp(verb, "stream") == 0 && arg && std::strcmp(arg, "off") == 0) {
        c.kind = CmdKind::StreamOff;
    } else if (std::strcmp(verb, "window") == 0) {
        if (!parseU32(arg, kWindowMinMs, kWindowMaxMs, c.value)) return invalid("window ms out of range");
        c.kind = CmdKind::Window;
    } else if (std::strcmp(verb, "shunt") == 0) {
        if (!parseU32(arg, kShuntMinUohm, kShuntMaxUohm, c.value)) return invalid("shunt uohm out of range");
        c.kind = CmdKind::Shunt;
    } else if (std::strcmp(verb, "sag") == 0) {
        if (!parseU32(arg, 0, kSagMaxMv, c.value)) return invalid("sag mv out of range");
        c.kind = CmdKind::Sag;
    } else if (std::strcmp(verb, "mark") == 0) {
        size_t n = arg ? std::strlen(arg) : 0;
        if (n == 0 || n >= sizeof c.label) return invalid("mark label length");
        for (size_t i = 0; i < n; i++)
            if (!labelChar(arg[i])) return invalid("mark label charset");
        std::memcpy(c.label, arg, n + 1);
        c.kind = CmdKind::Mark;
    } else {
        return invalid("unknown meter command");
    }
    return c;
}

}  // namespace meter
