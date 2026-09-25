/*
 * meter_window.h — per-window current/voltage aggregation and the `#PWR win`
 * line format (README "Output"). Pure C++, host-tested.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace meter {

/* INA226 LSBs: shunt 2.5 µV, bus 1.25 mV. */
constexpr int64_t kShuntLsbNv = 2500;
inline int32_t busRawToMv(uint16_t raw) { return static_cast<int32_t>(raw) * 5 / 4; }

struct Window {
    int64_t t0_us = 0;
    uint32_t n = 0;           /* shunt samples */
    int64_t sum = 0;          /* raw shunt counts */
    int32_t min = 0, max = 0;
    uint32_t clip = 0;        /* samples at the ADC's full scale */
    int32_t vbus_mv = -1;     /* latest; -1 = never read */
    int32_t vbus_min_mv = -1;
};

void windowReset(Window &w, int64_t t0_us);
void windowAddShunt(Window &w, int16_t raw);
void windowAddBus(Window &w, int32_t mv);

/* raw shunt counts -> µA through `shunt_uohm`. */
int64_t rawToMicroamps(int64_t raw, uint32_t shunt_uohm);

struct WindowFlags {
    bool sag;           /* INA226 bus-under-voltage alert latched */
    uint32_t cuts;      /* cumulative probe lines cut (meter_mux.h) */
    uint32_t txdrop;    /* cumulative host bytes dropped while probe down */
    uint32_t i2c_err;   /* cumulative */
    uint32_t lost;      /* cumulative meter lines that did not fit */
};

/* Writes one `#PWR win …` line (no newline). Returns length, or -1 if it did not fit. */
int formatWindow(char *buf, size_t cap, const Window &w, int64_t t1_us,
                 uint32_t shunt_uohm, const WindowFlags &f);

}  // namespace meter
