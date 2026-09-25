/*
 * display_text.h — what the OLED shows, as text. Pure C++, host-tested;
 * oled.cpp only draws these lines.
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace meter {

struct DisplaySnapshot {
    bool valid = false;        /* at least one window closed */
    const char *chip = "none"; /* "ina226" / "ina228" / "none" */
    bool probe_up = false;
    bool stream = false;
    int64_t mean_ua = 0, min_ua = 0, max_ua = 0;
    int32_t vbus_mv = -1, vbus_min_mv = -1;
    bool sag = false, clip = false;
    uint32_t i2c_err = 0;
    char label[33] = {};       /* last #mark, if any */
};

constexpr size_t kLineCap = 22;   /* 21 columns at 6 px + NUL */

struct DisplayLines {
    char head[kLineCap];    /* chip, rail, probe state */
    char big[kLineCap];     /* mean current, large font */
    char range[kLineCap];   /* min / peak */
    char flags[kLineCap];   /* rail minimum, sag, clip, i2c */
    char label[kLineCap];   /* sequence state or stream state */
};

/* µA -> "123.4" mA text, or "12.34" below 100 mA. */
void formatMa(char *buf, size_t cap, int64_t ua);
void composeLines(const DisplaySnapshot &s, DisplayLines &out);

}  // namespace meter
