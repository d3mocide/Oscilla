/*
 * ocp_csv.cpp — see ocp_csv.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ocp_csv.h"

#include "ocp.h"
#include "ocp_text.h"

namespace ocp {

namespace {

bool decodeInto(std::string_view field, std::vector<std::string> &out)
{
    while (!field.empty() && (field.front() == ' ' || field.front() == '\t')) field.remove_prefix(1);
    while (!field.empty() && (field.back() == ' ' || field.back() == '\t')) field.remove_suffix(1);

    uint8_t buf[OCP_MAX_LINE_LEN];
    int n = ocp_unescape_field(field.data(), field.size(), buf, sizeof buf);
    if (n < 0) return false;
    out.emplace_back(reinterpret_cast<const char *>(buf), static_cast<size_t>(n));
    return true;
}

}  // namespace

bool splitCsvRow(std::string_view row, std::vector<std::string> &out)
{
    out.clear();
    size_t start = 0;
    bool in_quote = false;

    for (size_t i = 0; i < row.size(); ++i) {
        char c = row[i];
        if (in_quote) {
            if (c == '\\' && i + 1 < row.size()) { ++i; continue; }
            if (c == '"') in_quote = false;
            continue;
        }
        if (c == '"') {
            in_quote = true;
        } else if (c == ',') {
            if (!decodeInto(row.substr(start, i - start), out)) { out.clear(); return false; }
            start = i + 1;
        }
    }
    if (in_quote || !decodeInto(row.substr(start), out)) { out.clear(); return false; }
    return true;
}

}  // namespace ocp
