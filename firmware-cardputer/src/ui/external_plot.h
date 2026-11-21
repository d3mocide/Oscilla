/* Measured packet-window geometry; no fabricated samples between windows. */
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include "model/spectrum_model.h"

namespace ui { namespace plot {
struct Point { int x = 0, y = 0; bool connected = false; };
struct Scope {
    std::array<Point, 32> points{};
    size_t size = 0;
    uint32_t peak = 0, scale = 1;
};

inline uint32_t scaleFor(uint32_t peak)
{
    uint32_t scale = 8;
    while (scale < peak && scale <= UINT32_MAX / 2) scale *= 2;
    return std::max(scale, peak);
}

inline Scope scope(const model::ChannelReading &r, uint32_t now, bool live)
{
    Scope out;
    const size_t count = std::min(r.history_size, r.history.size());
    const size_t first = (r.history_next + r.history.size() - count) % r.history.size();
    if (!live && count) now = r.history[(first + count - 1) % r.history.size()].received_ms;
    for (size_t i = 0; i < count; ++i) {
        const auto &sample = r.history[(first + i) % r.history.size()];
        if (now - sample.received_ms < 30000) out.peak = std::max(out.peak, sample.pkts);
    }
    out.scale = scaleFor(out.peak);
    uint32_t previous_ms = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto &sample = r.history[(first + i) % r.history.size()];
        const uint32_t age = now - sample.received_ms;
        if (age >= 30000) continue;
        auto &p = out.points[out.size];
        p.x = 20 + uint64_t(30000 - age) * 280 / 30000;
        p.y = 132 - uint64_t(sample.pkts) * 84 / out.scale;
        p.connected = out.size && sample.received_ms - previous_ms <= 1600;
        previous_ms = sample.received_ms;
        ++out.size;
    }
    return out;
}
} }
