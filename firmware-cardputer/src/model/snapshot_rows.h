/* Replace a complete snapshot without overlapping old/new table storage. */
#pragma once
#include <algorithm>
#include <utility>
#include <vector>
#include "ocp/ocp_item.h"

namespace model {
template<class Row, class Parse>
uint16_t replaceSnapshotRows(std::vector<Row> &rows, const ocp::Item &frame,
                             size_t limit, Parse parse) {
    const size_t needed = std::min(frame.rows.size(), limit);
    if (needed > rows.capacity()) {
        std::vector<Row>().swap(rows);
        rows.reserve(needed);
    }
    size_t written = 0;
    uint16_t malformed = 0;
    for (const auto &raw : frame.rows) {
        if (written == limit) break;
        Row row;
        if (!parse(raw, row)) { ++malformed; continue; }
        if (written < rows.size()) rows[written] = std::move(row);
        else rows.push_back(std::move(row));
        ++written;
    }
    rows.resize(written);
    return malformed;
}
}
