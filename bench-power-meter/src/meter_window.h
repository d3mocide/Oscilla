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

/* Per-chip scaling: ina_decode.h. */
struct ShuntScale {
    int64_t lsb_pv;
    int32_t full_scale;
};

struct Window {
    int64_t t0_us = 0;
    uint32_t n = 0;           /* shunt samples */
    int64_t sum = 0;          /* raw shunt counts, chip units */
    int32_t min = 0, max = 0;
    uint32_t clip = 0;        /* samples at the ADC's full scale */
    int32_t vbus_mv = -1;     /* latest; -1 = never read */
    int32_t vbus_min_mv = -1;
    bool sag = false;         /* bus-under-voltage alert seen */
};

void windowReset(Window &w, int64_t t0_us);
void windowAddShunt(Window &w, int32_t raw, const ShuntScale &sc);
void windowAddBus(Window &w, int32_t mv);
void windowAddSag(Window &w, bool sag);

/* raw shunt counts -> µA through `shunt_uohm`. */
int64_t rawToMicroamps(int64_t raw, int64_t lsb_pv, uint32_t shunt_uohm);

struct WindowFlags {
    uint32_t cuts;      /* cumulative probe lines cut (meter_mux.h) */
    uint32_t txdrop;    /* cumulative host bytes dropped while probe down */
    uint32_t i2c_err;   /* cumulative */
    uint32_t lost;      /* cumulative meter lines that did not fit */
};

/* Writes one `#PWR win …` line (no newline). Returns length, or -1 if it did not fit. */
int formatWindow(char *buf, size_t cap, const Window &w, int64_t t1_us,
                 uint32_t shunt_uohm, int64_t lsb_pv, const WindowFlags &f);

}  // namespace meter
