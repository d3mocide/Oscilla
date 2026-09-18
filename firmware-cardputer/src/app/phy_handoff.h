/*
 * phy_handoff.h — acknowledgement-gated transitions on the shared PHY lane.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <functional>

namespace app {

class PhyHandoff {
public:
    using Start = std::function<void(uint32_t)>;

    enum class Request : uint8_t { StartNow, SendStop, WaitForStop };

    /* Keep only the latest intended start while a scoped stop is in flight. */
    Request request(bool phy_active, bool stop_pending, Start start);

    bool takeAfterStop(Start *out);
    void clear() { queued_ = nullptr; }
    bool queued() const { return static_cast<bool>(queued_); }

private:
    Start queued_;
};

}  // namespace app
