/*
 * meter_mux.cpp — see meter_mux.h.
 *
 * SPDX-License-Identifier: MIT
 */
#include "meter_mux.h"

#include <cstring>

namespace meter {

void Downlink::forward(const uint8_t *p, size_t n, int64_t now_us, Sink &host)
{
    if (n == 0) return;
    last_byte_us_ = now_us;
    size_t start = 0;
    for (size_t i = 0; i < n; i++) {
        if (p[i] != '\n') continue;
        host.write(p + start, i + 1 - start);
        start = i + 1;
        at_line_start_ = true;
        flush(host);
    }
    if (start < n) {
        host.write(p + start, n - start);
        at_line_start_ = false;
    }
}

bool Downlink::queue(const char *line)
{
    size_t len = std::strlen(line);
    if (pending_len_ + len + 1 > kPendingCap) return false;
    std::memcpy(pending_ + pending_len_, line, len);
    pending_len_ += len;
    pending_[pending_len_++] = '\n';
    return true;
}

void Downlink::service(int64_t now_us, Sink &host)
{
    if (pending_len_ == 0) return;
    if (!at_line_start_) {
        if (now_us - last_byte_us_ < kStallUs) return;
        cut(host);
    }
    flush(host);
}

void Downlink::probeGone(Sink &host)
{
    if (!at_line_start_) cut(host);
    flush(host);
}

void Downlink::flush(Sink &host)
{
    if (pending_len_ == 0) return;
    host.write(reinterpret_cast<const uint8_t *>(pending_), pending_len_);
    pending_len_ = 0;
}

void Downlink::cut(Sink &host)
{
    static const uint8_t nl = '\n';
    host.write(&nl, 1);
    at_line_start_ = true;
    cuts_++;
}

void Uplink::feed(const uint8_t *p, size_t n, Sink *probe, CommandSink &cmds)
{
    size_t run = 0;   /* start of the current pass-through run */
    auto pass = [&](size_t end) {
        if (end <= run) return;
        if (probe) probe->write(p + run, end - run);
        else dropped_ += end - run;
    };

    for (size_t i = 0; i < n; i++) {
        uint8_t b = p[i];
        if (in_cmd_) {
            if (b == '\n') {
                cmd_[cmd_len_] = '\0';
                cmds.command(cmd_, overflow_);
                in_cmd_ = false;
                at_line_start_ = true;
                run = i + 1;
            } else if (b != '\r') {
                if (cmd_len_ < kCmdCap) cmd_[cmd_len_++] = static_cast<char>(b);
                else overflow_ = true;
            }
            continue;
        }
        if (at_line_start_ && b == '#') {
            pass(i);
            in_cmd_ = true;
            cmd_len_ = 0;
            overflow_ = false;
            continue;
        }
        at_line_start_ = (b == '\n');
    }
    if (!in_cmd_) pass(n);
}

}  // namespace meter
