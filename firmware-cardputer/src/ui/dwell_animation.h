/*
 * dwell_animation.h — shared bounded-listen progress treatment.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

#include <M5Cardputer.h>

#include "ui/theme.h"

namespace ui {

inline void drawDwellAnimation(M5Canvas &d, uint32_t elapsed_ms)
{
    const char *dots[] = { "", ".", "..", "..." };
    const char *phase = dots[(elapsed_ms / 250) % 4];

    d.setTextColor(kCalibrationYellow, kVoidInk);
    d.setTextSize(1);
    /* Keep the timer column fixed while the ellipsis animates. */
    d.setCursor(72, kBodyTop + 47);
    d.printf("LISTENING%-3s %02us", phase, (unsigned)(elapsed_ms / 1000));
}

}  // namespace ui
