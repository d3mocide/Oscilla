/*
 * ocp_parser.h — byte-exact OCP reader. Mirrors tools/ocp.py OcpParser;
 * tools/check_deck_parser.py diffs the two on a shared fuzz corpus.
 *
 * Never throws and never grows past its caps: bad input yields Noise items.
 * Rules: protocol/OCP-SPEC.md.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "ocp.h"
#include "ocp_item.h"

namespace ocp {

class Parser {
public:
    using Sink = std::function<void(Item &&)>;

    explicit Parser(size_t max_line_len = OCP_MAX_LINE_LEN,
                    size_t max_frame_rows = OCP_MAX_FRAME_ROWS);

    /* Raw serial bytes, any chunking. */
    void feed(const uint8_t *data, size_t len, const Sink &sink);

    /* One line, without its terminator. */
    void feedLine(const std::string &line, const Sink &sink);

    /* Drop a half-read frame on timeout or reset. Returns true if one was open. */
    bool abandonOpenFrame();

    bool frameOpen() const { return open_; }
    const std::string &openTag() const { return frame_.tag; }
    size_t bufferedBytes() const { return buf_.size(); }
    size_t openRows() const { return open_ ? frame_.rows.size() : 0; }

private:
    size_t max_line_len_;
    size_t max_frame_rows_;
    std::string buf_;
    bool overlong_ = false;
    bool open_ = false;
    Item frame_;
};

}  // namespace ocp
