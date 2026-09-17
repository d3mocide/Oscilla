/*
 * wardrive_sessions.cpp — see wardrive_sessions.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "storage/wardrive_sessions.h"

namespace storage::sessions {

bool parse(const std::string &name, int *index, bool *is_kml)
{
    if (!index || !is_kml || name.size() != 14 || name.compare(0, 6, "drive_") != 0) return false;

    int value = 0;
    for (std::size_t i = 6; i < 10; ++i) {
        const char c = name[i];
        if (c < '0' || c > '9') return false;
        value = value * 10 + (c - '0');
    }
    if (value < kFirstIndex || value > kLastIndex || name[10] != '.') return false;

    if (name.compare(11, 3, "csv") == 0) *is_kml = false;
    else if (name.compare(11, 3, "kml") == 0) *is_kml = true;
    else return false;

    *index = value;
    return true;
}

int firstFree(const bool *used, std::size_t count)
{
    if (!used || count <= static_cast<std::size_t>(kFirstIndex)) return 0;
    for (int i = kFirstIndex; i <= kLastIndex && static_cast<std::size_t>(i) < count; ++i) {
        if (!used[i]) return i;
    }
    return 0;
}

}  // namespace storage::sessions
