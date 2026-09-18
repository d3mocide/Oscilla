/*
 * boot_screen.cpp — Lens Core mark and deck wordmark for startup.
 *
 * Geometry follows docs/brand/assets/lens-core.svg; keeping it as primitives
 * avoids an SVG parser and preserves the small firmware image.
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui/boot_screen.h"

#include "ui/canvas.h"
#include "ui/theme.h"

namespace ui {

namespace {

void drawLensCore(M5Canvas &d)
{
    constexpr int cx = 120;
    constexpr int cy = 50;

    /* Full mark, scaled to 50 px: hexagonal field and orbital paths. */
    constexpr int hy = 21;
    d.drawLine(cx, cy - hy, cx + 18, cy - 11, kFieldGreen);
    d.drawLine(cx + 18, cy - 11, cx + 18, cy + 11, kFieldGreen);
    d.drawLine(cx + 18, cy + 11, cx, cy + hy, kFieldGreen);
    d.drawLine(cx, cy + hy, cx - 18, cy + 11, kFieldGreen);
    d.drawLine(cx - 18, cy + 11, cx - 18, cy - 11, kFieldGreen);
    d.drawLine(cx - 18, cy - 11, cx, cy - hy, kFieldGreen);
    d.drawEllipse(cx, cy, 17, 7, kFieldGreen);
    d.drawEllipse(cx, cy, 7, 17, kFieldGreen);

    /* The approved lens path, reduced to a crisp eight-point outline. */
    constexpr int lx = 9;
    constexpr int ly = 6;
    const int points[][2] = {
        {cx - lx, cy}, {cx - 6, cy - ly}, {cx, cy - ly - 1},
        {cx + 6, cy - ly}, {cx + lx, cy}, {cx + 6, cy + ly},
        {cx, cy + ly + 1}, {cx - 6, cy + ly},
    };
    for (size_t i = 0; i < 8; ++i) {
        const size_t next = (i + 1) % 8;
        d.drawLine(points[i][0], points[i][1], points[next][0], points[next][1],
                   kPaperPhosphor);
    }
    d.fillCircle(cx, cy, 3, kCalibrationYellow);
}

}  // namespace

void drawBootScreen()
{
    M5Canvas &d = canvas();
    d.fillScreen(kVoidInk);

    drawLensCore(d);

    d.setTextColor(kPaperPhosphor, kVoidInk);
    d.setTextSize(2);
    d.setCursor(78, 88);
    d.print("OSCILLA");

    present();
}

}  // namespace ui
