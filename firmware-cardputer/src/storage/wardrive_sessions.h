/*
 * wardrive_sessions.h — pure wardrive filename/index rules.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <string>

namespace storage::sessions {

constexpr int kFirstIndex = 1;
constexpr int kLastIndex = 9999;

/* Accept only the exact filenames produced by wardrive_logger. */
bool parse(const std::string &name, int *index, bool *is_kml);

/* `used` is indexed by session number and has `count` entries. */
int firstFree(const bool *used, std::size_t count);

}  // namespace storage::sessions
