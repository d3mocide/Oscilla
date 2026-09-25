/*
 * meter_mux.h — shares the meter's one USB port between the probe's OCP
 * stream and the meter's own `#PWR` lines. Meter lines are only ever inserted
 * at a probe line boundary, so to an OCP parser they are ordinary noise
 * (OCP-SPEC §4.1). Pure C++, host-tested.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace meter {

struct Sink {
    virtual void write(const uint8_t *p, size_t n) = 0;
    virtual ~Sink() = default;
};

/* Probe -> host. */
class Downlink {
public:
    static constexpr size_t kPendingCap = 1024;
    /* A probe line open this long is cut so meter output cannot wait forever. */
    static constexpr int64_t kStallUs = 1000000;

    void forward(const uint8_t *p, size_t n, int64_t now_us, Sink &host);
    /* Queue one meter line (no '\n'). False if it does not fit; nothing queued. */
    bool queue(const char *line);
    bool pendingEmpty() const { return pending_len_ == 0; }
    /* Flush at a boundary, or cut a stalled probe line first. */
    void service(int64_t now_us, Sink &host);
    /* Probe lost power: its partial line is dead, terminate it. */
    void probeGone(Sink &host);
    uint32_t cuts() const { return cuts_; }

private:
    void flush(Sink &host);
    void cut(Sink &host);

    char pending_[kPendingCap];
    size_t pending_len_ = 0;
    bool at_line_start_ = true;
    int64_t last_byte_us_ = 0;
    uint32_t cuts_ = 0;
};

struct CommandSink {
    /* `line` excludes the leading '#' and the newline; overflow = truncated. */
    virtual void command(const char *line, bool overflow) = 0;
    virtual ~CommandSink() = default;
};

/* Host -> probe. Lines starting with '#' are the meter's; the rest pass through. */
class Uplink {
public:
    static constexpr size_t kCmdCap = 96;

    /* probe == nullptr while the probe is unpowered: bytes are dropped, counted. */
    void feed(const uint8_t *p, size_t n, Sink *probe, CommandSink &cmds);
    uint32_t dropped() const { return dropped_; }

private:
    char cmd_[kCmdCap + 1];
    size_t cmd_len_ = 0;
    bool at_line_start_ = true;
    bool in_cmd_ = false;
    bool overflow_ = false;
    uint32_t dropped_ = 0;
};

}  // namespace meter
