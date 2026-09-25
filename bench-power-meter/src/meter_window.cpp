/*
 * meter_window.cpp — see meter_window.h.
 *
 * SPDX-License-Identifier: MIT
 */
#include "meter_window.h"

#include <cinttypes>
#include <cstdio>

namespace meter {

void windowReset(Window &w, int64_t t0_us)
{
    int32_t keep_vbus = w.vbus_mv;
    w = Window{};
    w.t0_us = t0_us;
    w.vbus_mv = keep_vbus;
    w.vbus_min_mv = keep_vbus;
}

void windowAddShunt(Window &w, int16_t raw)
{
    if (w.n == 0 || raw < w.min) w.min = raw;
    if (w.n == 0 || raw > w.max) w.max = raw;
    w.sum += raw;
    w.n++;
    if (raw == INT16_MAX || raw == INT16_MIN) w.clip++;
}

void windowAddBus(Window &w, int32_t mv)
{
    w.vbus_mv = mv;
    if (w.vbus_min_mv < 0 || mv < w.vbus_min_mv) w.vbus_min_mv = mv;
}

int64_t rawToMicroamps(int64_t raw, uint32_t shunt_uohm)
{
    /* µA = raw * 2.5 µV / (uohm µΩ) * 1e6 = raw * 2500 nV * 1000 / uohm. */
    return raw * kShuntLsbNv * 1000 / static_cast<int64_t>(shunt_uohm);
}

int formatWindow(char *buf, size_t cap, const Window &w, int64_t t1_us,
                 uint32_t shunt_uohm, const WindowFlags &f)
{
    int64_t mean = 0, lo = 0, hi = 0;
    if (w.n > 0) {
        mean = rawToMicroamps(w.sum, shunt_uohm) / static_cast<int64_t>(w.n);
        lo = rawToMicroamps(w.min, shunt_uohm);
        hi = rawToMicroamps(w.max, shunt_uohm);
    }
    int len = std::snprintf(
        buf, cap,
        "#PWR win t0_us=%" PRId64 " t1_us=%" PRId64 " n=%" PRIu32
        " i_ua=%" PRId64 " i_min_ua=%" PRId64 " i_max_ua=%" PRId64
        " vbus_mv=%" PRId32 " vbus_min_mv=%" PRId32 " sag=%d clip=%" PRIu32
        " cuts=%" PRIu32 " txdrop=%" PRIu32 " i2c_err=%" PRIu32 " lost=%" PRIu32,
        w.t0_us, t1_us, w.n, mean, lo, hi, w.vbus_mv, w.vbus_min_mv, f.sag ? 1 : 0,
        w.clip, f.cuts, f.txdrop, f.i2c_err, f.lost);
    if (len < 0 || static_cast<size_t>(len) >= cap) return -1;
    return len;
}

}  // namespace meter
