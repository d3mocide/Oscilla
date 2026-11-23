/*
 * wardrive_resume.h — when an open Drive log should restart the probe's
 * `start_wardrive` stream after another PHY tool interrupted it (OCP-SPEC §10.10).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace app {

class WardriveResume {
public:
    static constexpr uint32_t kSettleMs = 3000;

    /* True once `wanted` and `phy_clear` have both held for kSettleMs and
     * `can_send` is true. A PHY break restarts the wait; a busy client
     * (`can_send` false, e.g. a periodic status poll) only defers it. */
    bool due(bool wanted, bool phy_clear, bool can_send, uint32_t now_ms)
    {
        if (!wanted || !phy_clear) { clear_ = false; return false; }
        if (!clear_) { clear_ = true; since_ms_ = now_ms; return false; }
        return can_send && now_ms - since_ms_ >= kSettleMs;
    }
    void reset() { clear_ = false; }

private:
    bool clear_ = false;
    uint32_t since_ms_ = 0;
};

}  // namespace app
