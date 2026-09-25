/*
 * probe_power.cpp — see probe_power.h.
 *
 * SPDX-License-Identifier: MIT
 */
#include "probe_power.h"

namespace meter {

ProbePower::Change ProbePower::update(int32_t vbus_mv, int64_t now_us)
{
    if (up_) {
        if (vbus_mv >= kDownMv) return Change::None;
        up_ = false;
        rising_ = false;
        return Change::Down;
    }
    if (vbus_mv < kUpMv) {
        rising_ = false;
        return Change::None;
    }
    if (!rising_) {
        rising_ = true;
        rise_us_ = now_us;
    }
    if (now_us - rise_us_ < kUpHoldUs) return Change::None;
    up_ = true;
    rising_ = false;
    return Change::Up;
}

}  // namespace meter
