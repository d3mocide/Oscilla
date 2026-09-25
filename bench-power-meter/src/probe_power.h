/*
 * probe_power.h — decides from the measured rail whether the probe is
 * powered, so the meter only drives its UART into a live probe (Rev D §9
 * power sequencing). Pure C++, host-tested.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>

namespace meter {

class ProbePower {
public:
    enum class Change { None, Up, Down };

    /* Thresholds suit both a 3.3 V and a 5 V feed. */
    static constexpr int32_t kUpMv = 3000;
    static constexpr int32_t kDownMv = 2700;
    static constexpr int64_t kUpHoldUs = 200000;

    /* vbus_mv < 0 means the reading failed: treated as unpowered. */
    Change update(int32_t vbus_mv, int64_t now_us);
    bool up() const { return up_; }

private:
    bool up_ = false;
    bool rising_ = false;
    int64_t rise_us_ = 0;
};

}  // namespace meter
