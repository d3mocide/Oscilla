/*
 * idle_sleep.h — pure-C++ display idle-sleep state machine (DESIGN §7.1, P8).
 *
 * Keeps radio engines, GNSS, and SD logging alive while blanking both displays
 * after a configurable period with no key press. Any key wakes the displays
 * and is swallowed so the waking press never stops a scan or navigates away.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace app {

enum class IdleTransition : uint8_t { None, EnterSleep, Wake };

class IdleSleep {
public:
    static constexpr std::array<uint16_t, 5> kPresets = {0, 30, 60, 120, 300};

    static bool isValidTimeoutS(uint16_t seconds)
    {
        for (uint16_t p : kPresets) {
            if (p == seconds) return true;
        }
        return false;
    }

    static uint16_t sanitizeTimeoutS(uint16_t seconds)
    {
        return isValidTimeoutS(seconds) ? seconds : 0;
    }

    static const char *label(uint16_t seconds)
    {
        switch (sanitizeTimeoutS(seconds)) {
        case 30: return "30s";
        case 60: return "60s";
        case 120: return "2m";
        case 300: return "5m";
        default: return "OFF";
        }
    }

    void begin(uint32_t now_ms)
    {
        last_activity_ms_ = now_ms;
        sleeping_ = false;
    }

    uint16_t timeoutS() const { return timeout_s_; }
    bool sleeping() const { return sleeping_; }

    uint32_t idleMs(uint32_t now_ms) const { return now_ms - last_activity_ms_; }

    bool setTimeoutS(uint16_t seconds, uint32_t now_ms)
    {
        if (!isValidTimeoutS(seconds)) return false;
        timeout_s_ = seconds;
        last_activity_ms_ = now_ms;
        return true;
    }

    uint16_t cycleTimeoutS(int direction, uint32_t now_ms)
    {
        size_t idx = 0;
        for (size_t i = 0; i < kPresets.size(); ++i) {
            if (kPresets[i] == timeout_s_) { idx = i; break; }
        }
        const int n = static_cast<int>(kPresets.size());
        const int next = (static_cast<int>(idx) + (direction >= 0 ? 1 : -1) + n) % n;
        timeout_s_ = kPresets[static_cast<size_t>(next)];
        last_activity_ms_ = now_ms;
        return timeout_s_;
    }

    /* Call on any key press. Returns true when this press woke the display
     * and must be swallowed instead of dispatched to the active view. */
    bool onActivity(uint32_t now_ms)
    {
        last_activity_ms_ = now_ms;
        if (!sleeping_) return false;
        sleeping_ = false;
        return true;
    }

    IdleTransition tick(uint32_t now_ms)
    {
        if (timeout_s_ == 0) {
            if (!sleeping_) return IdleTransition::None;
            sleeping_ = false;
            return IdleTransition::Wake;
        }
        if (sleeping_) return IdleTransition::None;
        const uint32_t limit_ms = static_cast<uint32_t>(timeout_s_) * 1000UL;
        if (static_cast<uint32_t>(now_ms - last_activity_ms_) >= limit_ms) {
            sleeping_ = true;
            return IdleTransition::EnterSleep;
        }
        return IdleTransition::None;
    }

private:
    uint16_t timeout_s_ = 0;
    uint32_t last_activity_ms_ = 0;
    bool sleeping_ = false;
};

}  // namespace app
