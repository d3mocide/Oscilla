/*
 * line_reader.cpp — see line_reader.h.
 *
 * SPDX-License-Identifier: MIT
 */

#include "debug/line_reader.h"

namespace debug {

void LineReader::feed(const uint8_t *data, size_t len, const Sink &sink)
{
    for (size_t i = 0; i < len; ++i) {
        char b = static_cast<char>(data[i]);
        if (b == '\n') {
            std::string line;
            line.swap(buf_);
            bool was_overlong = overlong_;
            overlong_ = false;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!was_overlong) sink(line);
            continue;
        }
        if (overlong_) continue;   /* discard to end of line */
        buf_.push_back(b);
        if (buf_.size() > kMaxLineLen) {
            overlong_ = true;
            buf_.clear();
        }
    }
}

}  // namespace debug
