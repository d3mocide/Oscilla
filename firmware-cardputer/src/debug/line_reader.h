/*
 * line_reader.h — reassembles arbitrary-chunked bytes into '\n'-terminated
 * lines for the deck's debug console (DESIGN §7.6). Same never-trust-the-
 * chunking posture as gnss::NmeaParser: a line over kMaxLineLen is bounded
 * and dropped, not left to grow forever, and a human typing one byte per
 * keypress must still parse identically to the same bytes arriving in one
 * chunk. Framework-agnostic; host-tested in test/host/line_reader_test.cpp.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace debug {

class LineReader {
public:
    /* Debug commands are short (`wardrive`, `stop lora`); this is generous
     * headroom, not a real limit on anything. */
    static constexpr size_t kMaxLineLen = 128;

    using Sink = std::function<void(const std::string &)>;

    /* Raw bytes, any chunking. Emits one trimmed line (trailing '\r'
     * stripped) per '\n', including empty ones — filtering those is the
     * dispatcher's call, not this reader's. */
    void feed(const uint8_t *data, size_t len, const Sink &sink);

private:
    std::string buf_;
    bool overlong_ = false;
};

}  // namespace debug
