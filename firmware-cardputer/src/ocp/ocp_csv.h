/*
 * ocp_csv.h — split a [SCAN]-family row into decoded fields (OCP-SPEC §6, §10.2).
 * Mirrors tools/ocp.py split_csv_row; tools/check_deck_parser.py diffs them.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ocp {

/* False on an unterminated quote or a malformed escape; `out` is then empty. */
bool splitCsvRow(std::string_view row, std::vector<std::string> &out);

}  // namespace ocp
